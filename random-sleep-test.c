#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

/*
 * This is just testing how to sleep for a random amount of time.
 */
int main(int argc, char *argv[]) {
    srand(time(NULL));
    while (1) {
        // Random number between 5 and 10
        int randomNumber = (rand() % 10 + 1 - 5) + 5;
        printf("Random Number: %d\n", randomNumber);
        sleep(randomNumber);
    }
    return 0;
}

