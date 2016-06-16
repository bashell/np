#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <libgen.h>

#define ERR_EXIT(m) \
    do { \
        perror(m);\
        exit(EXIT_FAILURE);\
    }while(0)

#define MAX_EVENT_NUMBER 1024

static int pipefd[2];

int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd) {
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = EPOLLIN | EPOLLET;
    if(epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) == -1)
        ERR_EXIT("epoll_ctl");
    setnonblocking(fd);
}

void sig_handler(int sig) {
    int save_errno = errno;  // 保证可重入性
    int msg = sig;
    send(pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}

void addsig(int sig) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    if(sigaction(sig, &sa, NULL) == -1)
        ERR_EXIT("sigaction");
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

    struct epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    if(epollfd == -1)
        ERR_EXIT("epoll_create");
    addfd(epollfd, sockfd);

    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, pipefd);
    if(ret == -1)
        ERR_EXIT("socketpair");
    setnonblocking(pipefd[1]);  // 非阻塞写
    addfd(epollfd, pipefd[0]);  // 监听读端

    addsig(SIGHUP);
    addsig(SIGCHLD);
    addsig(SIGTERM);
    addsig(SIGINT);
    int stop_server = 0;

    while(!stop_server) {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if(number == -1 && errno != EINTR)
            ERR_EXIT("epoll_wait");
        int i;
        for(i = 0; i < number; ++i) {
            int tmpfd = events[i].data.fd;
            if(tmpfd == sockfd) {  // 处理新的连接
                struct sockaddr_in client;
                socklen_t client_len = sizeof(client);
                int connfd = accept(sockfd, (struct sockaddr*)&client, &client_len);
                if(connfd == -1)
                    ERR_EXIT("accept");
                addfd(epollfd, connfd);
            } else if(tmpfd == pipefd[0] && events[i].events & EPOLLIN) {  // 管道可读
                int sig;
                char signals[1024];
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if(ret == -1)
                    continue;
                else if(ret == 0)
                    continue;
                else {
                    int j;
                    for(j = 0; j < ret; ++j) {
                        switch(signals[j]) {
                            case SIGCHLD:
                            case SIGHUP:
                                continue;
                            case SIGTERM:
                            case SIGINT:
                                stop_server = 1;
                        }
                    }
                }
            } else {

            }
        }
    }
    printf("close fds\n");
    close(sockfd);
    close(pipefd[1]);
    close(pipefd[0]);
    exit(EXIT_FAILURE);
}
