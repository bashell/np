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

#define MAX_EVENT_NUMBER 1024
#define TCP_BUFFER_SIZE 512
#define UDP_BUFFER_SIZE 1024

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

void addfd(int epollfd, int fd) {
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;  // ET
    if(epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event) == -1)
        ERR_EXIT("epoll_ctl");
    setnonblocking(fd);
}

int main(int argc, char *argv[])
{
    if(argc <= 2) {
        printf("Usage: %s IP PORT\n", basename(argv[0]));
        exit(EXIT_FAILURE);
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    int ret = 0;

    // TCP
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1)
        ERR_EXIT("socket");
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);
    if(bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) 
        ERR_EXIT("bind");
    if(listen(sockfd, SOMAXCONN) == -1)
        ERR_EXIT("listen");

    // UDP
    int udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(udpfd == -1)
        ERR_EXIT("socket");
    struct sockaddr_in udp_addr;
    memset(&udp_addr, 0, sizeof(udp_addr));
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_port = htons(port);
    udp_addr.sin_addr.s_addr = inet_addr(ip);
    if(bind(udpfd, (struct sockaddr*)&udp_addr, sizeof(udp_addr)) == -1)
        ERR_EXIT("bind");

    struct epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);  // epoll句柄
    if(epollfd == -1)
        ERR_EXIT("epoll_create");

    // 注册TCP和UDP上的可读事件
    addfd(epollfd, sockfd);
    addfd(epollfd, udpfd);

    for(;;) {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if(number == -1)
            ERR_EXIT("epoll_wait");
        int i;
        for(i = 0; i < number; ++i) {
            int fd = events[i].data.fd;
            if(fd == sockfd) {  // TCP请求
                struct sockaddr_in client;
                socklen_t client_len = sizeof(client);
                int connfd = accept(sockfd, (struct sockaddr*)&client, &client_len);
                if(connfd == -1)
                    ERR_EXIT("accept");
                addfd(epollfd, connfd);
            } else if(fd == udpfd) {  // UDP请求
                char buf[UDP_BUFFER_SIZE];
                memset(buf, 0, UDP_BUFFER_SIZE);
                struct sockaddr_in client;
                socklen_t client_len = sizeof(client);
                ret = recvfrom(udpfd, buf, UDP_BUFFER_SIZE-1, 0, (struct sockaddr*)&client, &client_len);
                if(ret > 0)
                    sendto(udpfd, buf, UDP_BUFFER_SIZE-1, 0, (struct sockaddr*)&client, client_len);
            } else if(events[i].events & EPOLLIN) {  // 可读事件发生
                char buf[TCP_BUFFER_SIZE];
                for(;;) {
                    memset(buf, 0, TCP_BUFFER_SIZE);
                    ret = recv(fd, buf, TCP_BUFFER_SIZE-1, 0);
                    if(ret < 0) {
                        if((errno == EAGAIN) || (errno == EWOULDBLOCK))
                            break;
                        close(fd);
                        break;
                    } else if(ret == 0) {
                        close(fd);
                    } else {
                        send(fd, buf, ret, 0);
                    }
                }
            } else {
                printf("something else happened\n");
            }
        }
    }
    close(sockfd);
    close(udpfd);
    return 0;
}
