#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <libgen.h>
#include <pthread.h>

#define ERR_EXIT(m) \
    do { \
        perror(m);\
        exit(EXIT_FAILURE);\
    }while(0)

#define MAX_EVENT_NUM 1024
#define BUFFER_SIZE 1024

typedef struct tag {
    int epollfd;
    int sockfd;
}fds;

int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd, int oneshot) {
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    if(oneshot == 1)
        event.events |= EPOLLONESHOT;
    if(epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event) == -1)
        ERR_EXIT("epoll_ctl");
    setnonblocking(fd);
}

void reset_oneshot(int epollfd, int fd) {
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    if(epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event) == -1)
        ERR_EXIT("epoll_ctl");
}

void* worker(void* arg) {
    int epollfd = ((fds*)arg)->epollfd;
    int sockfd = ((fds*)arg)->sockfd;
    printf("start new thread to receive data on fd: %d\n", sockfd);
    char buf[BUFFER_SIZE];
    memset(buf, 0, BUFFER_SIZE);
    while(1) {
        int ret = recv(sockfd, buf, BUFFER_SIZE-1, 0);
        if(ret == 0) {
            close(sockfd);
            printf("foreiner closed the connection\n");
            break;
        } else if(ret < 0) {
            if(errno == EAGAIN) {
                reset_oneshot(epollfd, sockfd);
                printf("read later\n");
                break;
            }
        } else {
            printf("get content: %s\n", buf);
            sleep(5);  // 处理过程
        }
    }
    printf("end thread receiving data on fd: %d\n", sockfd);
}

int main(int argc, char *argv[])
{
    if(argc <= 2) {
        printf("Usage: %s IP PORT\n", basename(argv[0]));
        exit(EXIT_FAILURE);
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd == -1)
        ERR_EXIT("socket");

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = inet_addr(ip);

    if(bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1)
        ERR_EXIT("bind");
    if(listen(listenfd, SOMAXCONN) == -1)
        ERR_EXIT("listen");

    struct epoll_event events[MAX_EVENT_NUM];
    int epollfd = epoll_create(5);
    if(epollfd == -1)
        ERR_EXIT("epoll_create");
    addfd(epollfd, listenfd, 0);
    while(1) {
        int i;
        int ret = epoll_wait(epollfd, events, MAX_EVENT_NUM, -1);
        if(ret == -1)
            ERR_EXIT("epoll_wait");
        for(i = 0; i < ret; ++i) {
            int sockfd = events[i].data.fd;
            if(sockfd == listenfd) {
                struct sockaddr_in client;
                socklen_t client_len = sizeof(client);
                int connfd = accept(listenfd, (struct sockaddr*)&client, &client_len);
                if(connfd == -1)
                    ERR_EXIT("accept");
                addfd(epollfd, connfd, 1);
            } else if(events[i].events & EPOLLIN) {
                pthread_t thread;
                fds fds_for_new_worker;
                fds_for_new_worker.epollfd = epollfd;
                fds_for_new_worker.sockfd = sockfd;
                pthread_create(&thread, NULL, worker, (void*)&fds_for_new_worker);
            } else {
                printf("something else happened\n");
            }
        }
    }
    close(listenfd);
    return 0;
}
