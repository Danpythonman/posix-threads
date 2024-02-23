#include <pthread.h>
#include "errors.h"

#define ITERATIONS 10

/*
 * Each thread must lock these three mutexes
 */
pthread_mutex_t mutex[3] = {
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_MUTEX_INITIALIZER
};

/*
 * If backoff is 1, we will use the backoff strategy.
 * Otherwise, no backoff strategy will be used (potentially causing a
 * deadlock).
 */
int backoff = 1;

/*
 * 0:  no yield
 * >0: yield between iterations (to let other threads execute)
 * <0: sleep between iterations (to be really sure other threads will
 *     execute)
 */
int yieldFlag = 0;

/*
 * Locks mutexes 1, 2, and 3, in that order. When the global variable
 * backoff is set, a backoff algorithm will be used so that there will
 * be no deadlocks with lock_backward (which locks the same three
 * mutexes in reverse order). When the backoff globabl variable is not
 * set, a deadlock will likely occur with lock_backward.
 */
void *lock_forward(void *arg) {
    int backoffs;
    int status;

    for (int i = 0; i < ITERATIONS; i++) {
        backoffs = 0;

        for (int j = 0; j < 3; j++) {
            if (j == 0) {
                printf("1:11\n");
                status = pthread_mutex_lock(&mutex[0]);
                if (status != 0)
                    err_abort(status, "First lock");
            } else {
                printf("1:22\n");
                if (backoff)
                    status = pthread_mutex_trylock(&mutex[j]);
                else
                    status = pthread_mutex_lock(&mutex[j]);

                if (status == EBUSY) {
                    backoffs++;
                    printf("[forward locker backing off at %d]\n", j);
                    // Unlock all mutexes that have been locked (in
                    // reverse order, to reduce the chance of further
                    // backoffs in other threads).
                    for ( ; j > 0; j--) {
                        printf("1un\n");
                        status = pthread_mutex_unlock(&mutex[j-1]);
                        if (status != 0)
                            err_abort(status, "Backoff");
                    }
                } else if (status != 0) {
                    err_abort(status, "Lock mutex");
                }
            }

            if (yieldFlag > 0)
                sched_yield();
            else if (yieldFlag < 0)
                sleep(1);
        }

        printf(
               "lock_forward got all mutexes, %d backoffs\n",
               backoffs);

        // Unlock all three mutexes in reverse order (to reduce chance
        // of other threads having to backoff).
        for (int k = 2; k >= 0; k--) {
            printf("1f\n");
            pthread_mutex_unlock(&mutex[k]);
        }
    }

    return NULL;
}

/*
 * Locks mutexes 3, 2, and 1, in that order. When the global variable
 * backoff is set, a backoff algorithm will be used so that there will
 * be no deadlocks with lock_forward (which locks the same three
 * mutexes in reverse order). When the backoff globabl variable is not
 * set, a deadlock will likely occur with lock_forward.
 */
void *lock_backward(void *arg) {
    int backoffs;
    int status;

    for (int i = 0; i < ITERATIONS; i++) {
        backoffs = 0;

        for (int j = 2; j >= 0; j--) {
            if (j == 2) {
                printf("2:11\n");
                status = pthread_mutex_lock(&mutex[2]);
                if (status != 0)
                    err_abort(status, "First lock");
            } else {
                printf("2:22\n");
                if (backoff)
                    status = pthread_mutex_trylock(&mutex[j]);
                else
                    status = pthread_mutex_lock(&mutex[j]);

                if (status == EBUSY) {
                    backoffs++;
                    printf(
                           "[backward locker backing off at %d]\n",
                           j
                           );
                    // Unlock all mutexes that have been locked (in
                    // reverse order, to reduce the chance of further
                    // backoffs in other threads).
                    for ( ; j < 2; j++) {
                        printf("2un\n");
                        status = pthread_mutex_unlock(&mutex[j+1]);
                        if (status != 0)
                            err_abort(status, "Backoff");
                    }
                } else if (status != 0) {
                    err_abort(status, "Lock mutex");
                }
            }

            if (yieldFlag > 0)
                sched_yield();
            else if (yieldFlag < 0)
                sleep(1);
        }

        printf(
               "lock_backward got all mutexes, %d backoffs\n",
               backoffs
               );

        // Unlock all three mutexes in reverse order (to reduce chance
        // of other threads having to backoff).
        for (int k = 0; k < 3; k++) {
            printf("2f\n");
            pthread_mutex_unlock(&mutex[k]);
        }
    }

    return NULL;
}


int main(int argc, char *argv[]) {
    pthread_t forward;
    pthread_t backward;
    int status;

    if (argc > 1)
        backoff = atoi(argv[1]);

    if (argc > 2)
        yieldFlag = atoi(argv[2]);

    status = pthread_create(&forward, NULL, lock_forward, NULL);
    if (status != 0)
        err_abort(status, "Create forward");

    status = pthread_create(&backward, NULL, lock_backward, NULL);
    if (status != 0)
        err_abort(status, "Create backward");

    pthread_exit(NULL);
}

