#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define MAXNITEMS    1000000
#define MAXNTHREADS  100

int nitems;
struct {
    pthread_mutex_t mutex;
    int buff[MAXNITEMS];
    int nput;
    int nval;
} shared = {  // static allocation
    PTHREAD_MUTEX_INITIALIZER
};

int myMin(int a, int b) {
    return a < b ? a : b;
}

void* produce(void *arg) {
    for( ; ; ) {
        pthread_mutex_lock(&shared.mutex);
        if(shared.nput >= nitems) {
            pthread_mutex_unlock(&shared.mutex);
            return NULL;  // array is full
        }
        shared.buff[shared.nput] = shared.nval;
        ++shared.nput;
        ++shared.nval;
        pthread_mutex_unlock(&shared.mutex);
        *((int*) arg) += 1;
    }
}

void consume_wait(int i) {
    for( ; ; ) {
        pthread_mutex_lock(&shared.mutex);
        if(i < shared.nput) {
            pthread_mutex_unlock(&shared.mutex);
            return ;
        }
        pthread_mutex_unlock(&shared.mutex);
    }
}

void *consume(void *arg) {
    int i;
    for(i = 0; i < nitems; ++i) {
        consume_wait(i);
        if(shared.buff[i] != i)
            printf("buff[%d] = %d\n", i, shared.buff[i]);
    }
    return NULL;
}

int main(int argc, const char *argv[]) {
    int i, nthreads, count[MAXNTHREADS];
    pthread_t tid_produce[MAXNTHREADS], tid_consume;
    if(argc != 3) {
        perror("Usage: producer-consumer1 <#item> <#threads>");
        exit(EXIT_FAILURE);
    }
    nitems = myMin(atoi(argv[1]), MAXNITEMS);
    nthreads = myMin(atoi(argv[2]), MAXNTHREADS);

    pthread_setconcurrency(nthreads + 1);

    // start all producer threads
    for(i = 0; i < nthreads; ++i) {
        count[i] = 0;
        pthread_create(&tid_produce[i], NULL, produce, &count[i]);
    }

    pthread_create(&tid_consume, NULL, consume, NULL);

    // wait for all producer threads
    for(i = 0; i < nthreads; ++i) {
        pthread_join(tid_produce[i], NULL);
        printf("count[%d] = %d\n", i, count[i]);
    }
    pthread_join(tid_consume, NULL);

    exit(EXIT_SUCCESS);
}
