#include <pthread.h>
#include <time.h>
#include "errors.h"

/**
 * Alarm data type.
 */
typedef struct {
    int    seconds;
    time_t time;
    char   message[64];
} alarm_t;

/**
 * Data type for Node of alarm linked list.
 */
typedef struct alarm_linked_list {
    alarm_t *alarm;
    struct alarm_linked_list *next;
} alarm_linked_list;

/**
 * Mutex that protects alarm_list.
 */
pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Condition variable that that signals changes to alarm_list.
 */
pthread_cond_t alarm_cond = PTHREAD_COND_INITIALIZER;

/**
 * Linked list that holds the alarms.
 */
alarm_linked_list *alarm_list = NULL;

/**
 * current_alarm is 0 if the thread that handles alarms is idle. If
 * the alarm-handling thread is not idle, then current_alarm will have
 * the expiration (timestamp) value of the alarm being handled.
 */
time_t current_alarm = 0;

/**
 * Print the list of alarms for debugging. This will only print if the
 * the -DDEBUG flag is enabled when compiling.
 */
void print_list() {
#ifdef DEBUG /* Only define if -DDEBUG flag enabled. */

    // Iterate through list, printing the contents of each alarm
    alarm_linked_list *node;
    printf("{");
    for (node = alarm_list; node != NULL; node = node->next) {
        printf("%lld (%lld) [\"%s\"]",
               (long long) node->alarm->time,
               (long long) node->alarm->time - time(NULL),
               node->alarm->message);
        // Put comma, unless it is the last item in the list
        if (node->next != NULL) {
            printf(", ");
        }
    }
    printf("}\n");

#endif
}

/**
 * Insert an alarm into the alarm list and possibly notify other
 * thread that an alarm has been inserted.
 *
 * Special considerations:
 *   - since this function updates the alarm list, THE ALARM LIST
 *     MUTEX MUST BE LOCKED BY THE CALLER OF THIS METHOD.
 *
 *   - list nodes for the alarm list are malloced, so keep in mind
 *     that they must be freed when consumed.
 */
void alarm_insert(alarm_t *alarm) {
    int status;
    alarm_linked_list *prev;
    alarm_linked_list *next;
    alarm_linked_list *new_alarm;

    /*
     * We have to allocate space for the linked list node because if
     * we used a local variable it would disappear when the function
     * ends (I learned this from a painful debugging session).
     */
    new_alarm = malloc(sizeof(alarm_linked_list));
    if (new_alarm == NULL)
        errno_abort("Allocate alarm linked list node");

    // Attach alarm to the linked list node
    new_alarm->alarm = alarm;

    prev = NULL;
    next = alarm_list;

    // Iterate until the end of the list
    while (next != NULL) {
        /*
         * When we find an alarm in the list that happens after the
         * new alarm, insert the new alarm before it.
         */
        if (alarm->time <= next->alarm->time) {
            /*
             * Set the new alarm node's next pointer to the node that
             * should be after it in the list.
             */
            new_alarm->next = next;

            /*
             * If prev is NULL, then we are at the beginning of the
             * list. In this case, set the list to this alarm so that
             * it is at the front.
             *
             * Otherwise, just make the prev alarm node point to the
             * new alarm node.
             */
            if (prev == NULL) {
                alarm_list = new_alarm;
            } else {
                prev->next = new_alarm;
            }
            break;
        }
        // Move forward in list
        prev = next;
        next = next->next;
    }

    /*
     * If next == NULL at this point, then either:
     *   - the above while loop did not execute (meaning that the list
     *     was empty),
     *
     *   OR
     *
     *   - the above while loop completed without inserted the new
     *     alarm (meaning that the new alarm needs to be inserted at
     *     the end of the list).
     */
    if (next == NULL) {
        if (prev == NULL) {
            /*
             * Case 1: list was empty so set alarm list to the new
             *         alarm list node.
             */
            alarm_list = new_alarm;
        } else {
            /*
             * Case 2: alarm needs to be inserted at the end of the
             *         list, so insert it there.
             */
            prev->next = new_alarm;
        }
        /*
         * In both cases, the new alarm is at the end of the list. So
         * the next node must be null.
         */
        new_alarm->next = NULL;
    }

    // Print list. This will only happen if debug flag is enabled.
    print_list();

    /*
     * If current_alarm is 0, then the other thread is idle. If
     * current_alarm is greater than the new alarm, then the other
     * thread is handling an alarm that happens after the new alarm.
     * In both cases, we should set current_alarm to the new alarm, so
     * that it gets handled now and, notify the other thread about
     * this.
     */
    if (current_alarm == 0 || alarm->time < current_alarm) {
        current_alarm = alarm->time;
        pthread_cond_signal(&alarm_cond);
    }
}

/**
 * Handles alarms
 */
void *alarm_thread(void *arg) {
    alarm_t *alarm;
    alarm_linked_list *alarm_l_l;
    struct timespec cond_time;
    time_t now;
    int status;
    int expired;

    pthread_mutex_lock(&alarm_mutex);

    while (1) {
        // Set current_alarm to 0 to notify that this thread is idle
        current_alarm = 0;

        // Wait for alarm_list to have a value.
        while (alarm_list == NULL) {
            pthread_cond_wait(&alarm_cond, &alarm_mutex);
        }

        // Make sure alarm_list has a value (this extra check protects
        // against spurious wakeups).
        if (alarm_list == NULL) {
            printf("Spurious wakeup\n");
        } else {
            // Get alarm
            alarm = alarm_list->alarm;
            // Get alarm node
            alarm_l_l = alarm_list;
            // Remove alarm node from list
            alarm_list = alarm_list->next;

            now = time(NULL);

            expired = 0;

            if (now >= alarm->time) {
                /*
                 * If "now" is after the alarm, then it has expired
                 * and we must print it.
                 */
                expired = 1;
            } else {
                /*
                 * If "now" is before the alarm, then we must wait for
                 * it to expire.
                 */
                cond_time.tv_sec = alarm->time;
                cond_time.tv_nsec = 0;

                current_alarm = alarm->time;

                /*
                 * Wait for timer to expire with a timed wait on the
                 * condition variable.
                 *
                 * We use a condition variable because while we are
                 * waiting, another alarm may be added to the list
                 * that must be handled befor the alarm currently
                 * being watied for.
                 */
                while (current_alarm == alarm->time) {
                    status = pthread_cond_timedwait(&alarm_cond,
                                                    &alarm_mutex,
                                                    &cond_time);
                    /*
                     * If we timed out, then the alarm has expired.
                     */
                    if (status == ETIMEDOUT) {
                        printf("Expired\n");
                        expired = 1;
                        break;
                    }
                }

                /*
                 * If alarm is not expired, then another alarm was
                 * added to the list that must be handled first. In
                 * this case, add the alarm that got interrupted back
                 * into the list.
                 */
                if (!expired) {
                    alarm_insert(alarm);
                }
            }

            /*
             * If the alarm is expired, print it, free the alarm, free
             * the alarm linked list node.
             */
            if (expired) {
                printf("(%d) %s\n", alarm->seconds, alarm->message);
                free(alarm);
                free(alarm_l_l);
            }
        }
    }
}

/**
 * Main thread. Gets alarms from user and adds them to list.
 */
int main(int argc, char *argv[]) {
    int status;
    char line[128];
    alarm_t *alarm;
    pthread_t thread;

    // Create alarm-handling thread
    pthread_create(&thread, NULL, alarm_thread, NULL);

    while (1) {
        printf("Alarm > ");

        // Get line from user
        if (fgets(line, sizeof(line), stdin) == NULL)
            exit(0);

        // Make sure line had a value
        if (strlen(line) <= 1)
            continue;

        // Allocate memory for alarm (must be freed when handled)
        alarm = malloc(sizeof(alarm_t));
        if (alarm == NULL)
            errno_abort("Allocate alarm");

        // Parse the line, making sure alarm fits the correct format
        if (sscanf(line,
                   "%d %64[^\n]",
                   &alarm->seconds,
                   alarm->message) < 2)
        {
            fprintf(stderr, "Bad command\n");
            // Free alarm if the command had an invalid format,
            // because it will not be handled and freed later.
            free(alarm);
        } else {
            pthread_mutex_lock(&alarm_mutex);

            // Calculate absolute expiry time for the alarm.
            alarm->time = time(NULL) + alarm->seconds;

            // Insert the alarm into the list.
            alarm_insert(alarm);

            pthread_mutex_unlock(&alarm_mutex);
        }
    }
}

