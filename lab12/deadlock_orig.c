/* deadlock.c - two threads, two mutexes, taken in opposite order.
* Build: gcc -Wall -pthread -o deadlock deadlock.c
* Run: timeout 5 ./deadlock; echo "exit code: $?" (124 = killed by timeout)
*/
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
pthread_mutex_t A = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t B = PTHREAD_MUTEX_INITIALIZER;
void *one(void *arg) {
pthread_mutex_lock(&A);
printf("one: I have A, I want B\n");
pthread_mutex_lock(&B); /* waits for two */
printf("one: I have both\n");
pthread_mutex_unlock(&B);
pthread_mutex_unlock(&A);
return NULL;
}
void *two(void *arg) {
pthread_mutex_lock(&B); /* the fix: lock A first here too */
printf("two: I have B, I want A\n");
pthread_mutex_lock(&A); /* waits for one */
printf("two: I have both\n");
pthread_mutex_unlock(&A);
pthread_mutex_unlock(&B);
return NULL;
}
int main(void) {
pthread_t t1, t2;
pthread_create(&t1, NULL, one, NULL);
pthread_create(&t2, NULL, two, NULL);
pthread_join(t1, NULL);
pthread_join(t2, NULL);
printf("done\n");
return 0;
}
