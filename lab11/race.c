#include <stdio.h>
#include <pthread.h>

#define N 1000000

long counter = 0;               /* shared by both threads */

void *add(void *arg) {
    for (int i = 0; i < N; i++)
        counter++;              /* read, add 1, write back */
    return NULL;
}

int main(void) {
    pthread_t a, b;
    pthread_create(&a, NULL, add, NULL);
    pthread_create(&b, NULL, add, NULL);
    pthread_join(a, NULL);
    pthread_join(b, NULL);
    printf("counter = %ld (expected %d)\n", counter, 2 * N);
    return 0;
}
