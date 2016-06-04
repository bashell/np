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
#include <libgen.h>
#include <sys/epoll.h>

#define MAX_EVENT_NUM 1024
#define BUFFER_SIZE 10

#define ERR_EXIT(m) \
    do { \
        perror(m);\
        exit(EXIT_FAILURE);\
    }while(0)

int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd, int enable_et) {
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN;
    if(enable_et == 1)
        event.events |= EPOLLET;
    if(epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event) == -1)
        ERR_EXIT("epoll_ctl");
    setnonblocking(fd);
}

void lt(struct epoll_event *events, int number, int epollfd, int listenfd) {
    char buf[BUFFER_SIZE];
    int i;
    for(i = 0; i < number; ++i) {
        int sockfd = events[i].data.fd;
        if(sockfd == listenfd) {
            struct sockaddr_in client;
            socklen_t client_len = sizeof(client);
            int connfd = accept(listenfd, (struct sockaddr*)&client, &client_len);
            if(connfd == -1)
                ERR_EXIT("accept");
            addfd(epollfd, connfd, 0);
        } else if(events[i].events & EPOLLIN) {
            printf("event trigger once\n");
            memset(buf, 0, BUFFER_SIZE);
            int ret = recv(sockfd, buf, BUFFER_SIZE-1, 0);
            if(ret <= 0) {
                close(sockfd);
                continue;
            }
            printf("Get %d bytes of content: %s\n", ret, buf);
        } else {
            printf("Something else happened\n");
        }
    }
}

void et(struct epoll_event *events, int number, int epollfd, int listenfd) {
    char buf[BUFFER_SIZE];
    int i;
    for(i = 0; i < number; ++i) {
        int sockfd = events[i].data.fd;
        if(sockfd == listenfd) {
            struct sockaddr_in client;
            socklen_t client_len = sizeof(client);
            int connfd = accept(listenfd, (struct sockaddr*)&client, &client_len);
            if(connfd == -1)
                ERR_EXIT("accept");
            addfd(epollfd, connfd, 1);
        } else if(events[i].events & EPOLLIN) {
            printf("event trigger once\n");
            while(1) {
                memset(buf, 0, BUFFER_SIZE);
                int ret = recv(sockfd, buf, BUFFER_SIZE-1, 0);
                if(ret < 0) {
                    if((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                        printf("read later\n");
                        break;
                    }
                    close(sockfd);
                    break;
                } else if(ret == 0) {
                    close(sockfd);
                } else {
                    printf("Get %d bytes of content: %s\n", ret, buf);
                }
            }
        } else {
            printf("Something else happened\n");
        }
    }
}

int main(int argc, char *argv[])
{
    if(argc <= 2) {
        printf("Usage: %s IP PORT\n", basename(argv[0]));
        exit(EXIT_FAILURE);
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1)
        ERR_EXIT("socket");

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = inet_addr(ip);

    if(bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1)
        ERR_EXIT("bind");
    if(listen(sockfd, SOMAXCONN) == -1)
        ERR_EXIT("listen");

    struct epoll_event events[MAX_EVENT_NUM];
    int epollfd = epoll_create(5);
    if(epollfd == -1)
        ERR_EXIT("epoll_create");
    addfd(epollfd, sockfd, 1);
    while(1) {
        int ret = epoll_wait(epollfd, events, MAX_EVENT_NUM, -1);
        if(ret < 0) {
            printf("epoll fail\n");
            break;
        }
        //lt(events, ret, epollfd, sockfd);
        et(events, ret, epollfd, sockfd);
    }
    close(sockfd);
    return 0;
}
