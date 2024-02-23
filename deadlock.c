#include <pthread.h>
#include <time.h>
#include "errors.h"

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;

/*
 * thread_1 sleeps, then locks mutex 1, then sleeps again, then locks
 * mutex 2, then unlocks both mutexes. This is repeated indefinitely
 * (although this will cause a deadlock when run with thread_2).
 */
void *thread_1(void *arg) {
    int status;
    int randomDuration;

    while (1) {
        randomDuration = rand() % (5 + 1 - 2) + 2;
        printf("1: Sleeping for %d seconds\n", randomDuration);
        sleep(randomDuration);

        printf("1: about to lock mutex 1\n");

        status = pthread_mutex_lock(&mutex1);
        if (status != 0)
            err_abort(status, "Thread 1 mutex 1");

        printf("1: locked mutex 1\n");

        randomDuration = rand() % (5 + 1 - 2) + 2;
        printf("1: Sleeping for %d seconds\n", randomDuration);
        sleep(randomDuration);

        printf("1: about to lock mutex 2\n");

        status = pthread_mutex_lock(&mutex2);
        if (status != 0)
            err_abort(status, "Thread 1 mutex 2");

        printf("1: locked mutex 2\n");

        printf("1: about to unlock mutex 1\n");

        status = pthread_mutex_unlock(&mutex1);
        if (status != 0)
            err_abort(status, "Thread 1 mutex 1 unlock");

        printf("1: unlocked mutex 1\n");

        printf("1: about to unlock mutex 2\n");

        status = pthread_mutex_unlock(&mutex2);
        if (status != 0)
            err_abort(status, "Thread 1 mutex 2 unlock");

        printf("1: unlcoked mutex 2\n");

        printf("1: finished\n");
    }

    return NULL;
}

/*
 * thread_2 sleeps, then locks mutex 2, then sleeps again, then locks
 * mutex 1, then unlocks both mutexes. This is repeated indefinitely
 * (although this will cause a deadlock when run with thread_1).
 */
void *thread_2(void *arg) {
    int status;
    int randomDuration;

    while (1) {
        randomDuration = rand() % (5 + 1 - 2) + 2;
        printf("2: Sleeping for %d seconds\n", randomDuration);
        sleep(randomDuration);

        printf("2: about to lock mutex 2\n");

        status = pthread_mutex_lock(&mutex2);
        if (status != 0)
            err_abort(status, "Thread 2 mutex 2");

        printf("2: locked mutex 2\n");

        randomDuration = rand() % (5 + 1 - 2) + 2;
        printf("2: Sleeping for %d seconds\n", randomDuration);
        sleep(randomDuration);

        printf("2: about to lock mutex 1\n");

        status = pthread_mutex_lock(&mutex1);
        if (status != 0)
            err_abort(status, "Thread 2 mutex 1");

        printf("2: locked mutex 1\n");

        printf("2: about to unlock mutex 1\n");

        status = pthread_mutex_unlock(&mutex1);
        if (status != 0)
            err_abort(status, "Thread 2 mutex 1 unlock");

        printf("2: unlocked mutex 1\n");

        printf("2: about to unlock mutex 2\n");

        status = pthread_mutex_unlock(&mutex2);
        if (status != 0)
            err_abort(status, "Thread 2 mutex 2 unlock");

        printf("2: unlcoked mutex 2\n");

        printf("2: finished\n");
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    int status;
    pthread_t thread_1_id;
    pthread_t thread_2_id;

    status = pthread_create(
                            &thread_1_id,
                            NULL,
                            thread_1,
                            NULL
                            );

    if (status != 0)
        err_abort(status, "Create thread 1");

    status = pthread_create(
                            &thread_2_id,
                            NULL,
                            thread_2,
                            NULL
                            );
    if (status != 0)
        err_abort(status, "Create thread 2");

    status = pthread_join(thread_1_id, NULL);
    if (status != 0)
        err_abort(status, "Join thread 1");

    status = pthread_join(thread_2_id, NULL);
    if (status != 0)
        err_abort(status, "Join thread 2");

    return 0;
}

