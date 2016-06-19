#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define MAXNITEMS    1000000
#define MAXNTHREADS  100

int nitems;
int buff[MAXNITEMS];

struct {
    pthread_mutex_t mutex;
    int nput;
    int nval;
} put = {
    PTHREAD_MUTEX_INITIALIZER
};

struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int nready;
} nready = {
    PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER
};

int myMin(int a, int b) {
    return a < b ? a : b;
}

void *produce(void *arg) {
    for( ; ; ) {
        pthread_mutex_lock(&put.mutex);
        if(put.nput >= nitems) {
            pthread_mutex_unlock(&put.mutex);
            return NULL;
        }
        buff[put.nput] = put.nval;
        ++put.nput;
        ++put.nval;
        pthread_mutex_unlock(&put.mutex);
        
        pthread_mutex_lock(&nready.mutex);
        if(nready.nready == 0) {
            pthread_cond_signal(&nready.cond);
        }
        ++nready.nready;
        pthread_mutex_unlock(&nready.mutex);

        *((int *) arg) += 1;
    }
}

void *consume(void *arg) {
    int i;
    for(i = 0; i < nitems; ++i) {
        pthread_mutex_lock(&nready.mutex);
        while(nready.nready == 0)
            pthread_cond_wait(&nready.cond, &nready.mutex);
        --nready.nready;
        pthread_mutex_unlock(&nready.mutex);

        if(buff[i] != i)
            printf("buff[%d] = %d\n", i, buff[i]);
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
