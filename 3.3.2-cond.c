#include <pthread.h>
#include <time.h>
#include "errors.h"

typedef struct {
    pthread_mutex_t mutex; // Protects access to value.
    pthread_cond_t  cond;  // Signals changes to value.
    int             value; // Value that is protected by mutex.
} value_cond_t;

value_cond_t data = {
    PTHREAD_MUTEX_INITIALIZER,
    PTHREAD_COND_INITIALIZER,
    0
};

/**
 * Time to sleep (in seconds).
 */
int sleep_time = 1;

void *wait_thread(void *arg) {
    int status;

    // Sleep
    sleep(sleep_time);

    // Lock the mutex
    status = pthread_mutex_lock(&data.mutex);
    if (status != 0)
        err_abort(status, "Lock mutex");

    // Update the value
    data.value = 1;

    // Signal via the condition variable that the value has changed
    status = pthread_cond_signal(&data.cond);
    if (status != 0)
        err_abort(status, "Signal condition");

    // Unlock the mutex
    status = pthread_mutex_unlock(&data.mutex);
    if (status != 0)
        err_abort(status, "Unlock mutex");

    return NULL;
}

int main(int argc, char *argv[]) {
    int status;
    pthread_t wait_thread_id;
    struct timespec timeout;

    if (argc > 1)
        sleep_time = atoi(argv[1]);

    status = pthread_create(&wait_thread_id, NULL, wait_thread, NULL);
    if (status != 0)
        err_abort(status, "Create wait thread");

    timeout.tv_sec = time(NULL) + 2;
    timeout.tv_nsec = 0;

    status = pthread_mutex_lock(&data.mutex);
    if (status != 0)
        err_abort(status, "Lock mutex");

    // Only leave the while loop if the value was in fact changed (or
    // if timeout).
    while (data.value == 0) {
        status = pthread_cond_timedwait(&data.cond,
                                        &data.mutex,
                                        &timeout);
        if (status == ETIMEDOUT) {
            printf("Condition wait timed out\n");
            break;
        } else if (status != 0) {
            err_abort(status, "Wait on condition");
        }
    }

    // Check value one more time before continuting (this protects
    // against spurious wakeups).
    if (data.value == 0)
        printf("Condition not met\n");
    else
        printf("Condition met\n");

    status = pthread_mutex_unlock(&data.mutex);
    if (status != 0)
        err_abort(status, "Unlock mutex");

    return 0;
}

