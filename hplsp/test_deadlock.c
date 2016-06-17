#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define ERR_EXIT(m) \
    do { \
        perror(m);\
        exit(EXIT_FAILURE);\
    }while(0)

int a = 0;
int b = 0;
pthread_mutex_t mutex_a;
pthread_mutex_t mutex_b;

void *another(void *arg) {
    if(pthread_mutex_lock(&mutex_b) != 0)
        ERR_EXIT("pthread_mutex_lock");
    printf("In child thread, get mutex b, waiting for mutex a\n");
    sleep(5);
    ++b;
    if(pthread_mutex_lock(&mutex_a) != 0)
        ERR_EXIT("pthread_mutex_lock");
    b += a++;
    if(pthread_mutex_unlock(&mutex_a) != 0)
        ERR_EXIT("pthread_mutex_unlock");
    if(pthread_mutex_unlock(&mutex_b) != 0)
        ERR_EXIT("pthread_mutex_unlock");
    pthread_exit(NULL);
}

int main()
{
    pthread_t tid;
    if(pthread_mutex_init(&mutex_a, NULL) != 0)
        ERR_EXIT("pthread_mutex_init");
    if(pthread_mutex_init(&mutex_b, NULL) != 0)
        ERR_EXIT("pthread_mutex_init");
    if(pthread_create(&tid, NULL, another, NULL) != 0)
        ERR_EXIT("pthread_create");
    
    if(pthread_mutex_lock(&mutex_a) != 0)
        ERR_EXIT("pthread_mutex_lock");
    printf("In parent thread, get mutex a, waiting for mutex b\n");
    sleep(5);
    ++a;
    if(pthread_mutex_lock(&mutex_b) != 0)
        ERR_EXIT("pthread_mutex_lock");
    a += b++;
    if(pthread_mutex_unlock(&mutex_b) != 0)
        ERR_EXIT("pthread_mutex_unlock");
    if(pthread_mutex_unlock(&mutex_a) != 0)
        ERR_EXIT("pthread_mutex_unlock");
    
    if(pthread_join(tid, NULL) != 0)
        ERR_EXIT("pthread_join");
    if(pthread_mutex_destroy(&mutex_a) != 0)
        ERR_EXIT("pthread_mutex_destroy");
    if(pthread_mutex_destroy(&mutex_b) != 0)
        ERR_EXIT("pthread_mutex_destroy");
    return 0;
}
