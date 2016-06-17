#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>

#define ERR_EXIT(m) \
    do { \
        perror(m);\
        exit(EXIT_FAILURE);\
    }while(0)

pthread_mutex_t mutex;

void *another(void *arg) {
    printf("In child thread, lock the mutex\n");
    pthread_mutex_lock(&mutex);
    sleep(5);
    pthread_mutex_unlock(&mutex);
}

int main()
{
    pthread_mutex_init(&mutex, NULL);
    pthread_t tid;
    pthread_create(&tid, NULL, another, NULL);
    sleep(1);
    int pid = fork();
    if(pid < 0) {
        pthread_join(tid, NULL);
        pthread_mutex_destroy(&mutex);
        exit(EXIT_FAILURE);
    } else if(pid == 0) {  // child
        printf("I am in the child, want to get the lock\n");
        pthread_mutex_lock(&mutex);  // deadlock
        printf("I can't run to here, ooop...\n");
        pthread_mutex_unlock(&mutex);
        exit(EXIT_SUCCESS);
    } else {  // parent
        wait(NULL);        
    }
    pthread_join(tid, NULL);
    pthread_mutex_destroy(&mutex);
    exit(EXIT_SUCCESS);
}
