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
#include <fcntl.h>
#include <sys/epoll.h>
#include <libgen.h>
#include "test_lst_timer.h"

#define ERR_EXIT(m) \
    do { \
        perror(m);\
        exit(EXIT_FAILURE);\
    }while(0)

#define FD_LIMIT 65535
#define MAX_EVENT_NUMBER 1024
#define TIMESLOT 5

static int pipefd[2];  // 管道用于传输信号通知
static sort_timer_lst timer_lst;  // 定时器升序链表
static int epollfd = 0;

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

/* 信号处理函数 */
void sig_handler(int sig) {
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1], (char*)&msg, 1, 0);  // 将信号值写入管道
    errno = save_errno;
}

/* 设置信号处理函数 */
void addsig(int sig) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    if(sigaction(sig, &sa, NULL) == -1)
        ERR_EXIT("sigaction");
}

void timer_handler() {
    timer_lst.tick();  // 定时处理任务
    alarm(TIMESLOT);   // 重新定时，不断触发SIGALRM信号
}

/* 定时器回调函数 */
void cb_func(client_data *user_data) {
    if(!user_data) return;
    // 删除非活动连接socket上的注册事件，并关闭fd
    if(epoll_ctl(epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0) == -1)
        ERR_EXIT("epoll_ctl");
    close(user_data->sockfd);
    printf("close fd %d\n", user_data->sockfd);
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

    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, pipefd);
    if(ret == -1)
        ERR_EXIT("socketpair");
    setnonblocking(pipefd[1]);
    addfd(epollfd, pipefd[0]);

    // 设置信号处理函数
    addsig(SIGALRM);
    addsig(SIGTERM);
    bool stop_server = false;

    client_data *users = new client_data[FD_LIMIT];
    if(users == NULL)
        ERR_EXIT("new");
    bool timeout = false;
    alarm(TIMESLOT);  // 定时

    while(!stop_server) {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if(number == -1 && errno != EINTR)
            ERR_EXIT("epoll_wait");
        for(int i = 0; i < number; ++i) {
            int tmpfd = events[i].data.fd;
            if(tmpfd == sockfd) {  // 处理新的客户连接
                struct sockaddr_in client;
                socklen_t client_len = sizeof(client);
                int connfd = accept(sockfd, (struct sockaddr*)&client, &client_len);
                if(connfd == -1)
                    ERR_EXIT("accept");
                addfd(epollfd, connfd);
                users[connfd].address = client;
                users[connfd].sockfd = connfd;

                // 创建定时器，并设置回调函数和超时时间，将定时器和用户数据进行绑定，然后将定时器添加到定时器链表中
                util_timer *timer = new util_timer;
                if(timer == NULL)
                    ERR_EXIT("new");
                time_t cur = time(NULL);
                timer->expire = cur + 3 * TIMESLOT;
                timer->cb_func = cb_func;
                timer->user_data = &users[connfd];
                users[connfd].timer = timer;
                timer_lst.add_timer(timer);
            } else if(tmpfd == pipefd[0] && events[i].events & EPOLLIN) {  // 处理信号
                int sig;
                char signals[1024];
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if(ret == -1)
                    continue;  // handle error
                else if(ret == 0)
                    continue;
                else {
                    for(int j = 0; j < ret; ++j) {
                        switch(signals[i]) {
                            case SIGALRM:
                                timeout = true;
                                break;
                            case SIGTERM:
                                stop_server = true;
                        }
                    }
                }
            } else if(events[i].events & EPOLLIN) {  // 处理客户连接上的数据
                memset(users[tmpfd].buf, 0, BUFFER_SIZE);
                ret = recv(tmpfd, users[tmpfd].buf, BUFFER_SIZE-1, 0);
                printf("get %d bytes of client data %s from %d\n", ret, users[tmpfd].buf, tmpfd);
                util_timer *timer = users[tmpfd].timer;
                if(ret == -1) {
                    if(errno != EAGAIN) {  // 发生读错误，关闭连接并移除定时器
                        cb_func(&users[tmpfd]);
                        if(timer)
                            timer_lst.del_timer(timer);
                    }
                } else if(ret == 0) {  // 对方关闭连接，本方也关闭连接并移除定时器
                    cb_func(&users[tmpfd]);
                    if(timer)
                        timer_lst.del_timer(timer);
                } else {  // 客户连接有数据可读
                    // 调整相应的定时器，延迟该连接被关闭的时间
                    if(timer) {
                        time_t cur = time(NULL);
                        timer->expire = cur + 3 * TIMESLOT;
                        printf("adjust time once\n");
                        timer_lst.adjust_timer(timer);
                    } else {
                        // other operations
                    }
                }
                // 处理定时事件
                if(timeout) {
                    timer_handler();
                    timeout = false;
                }
            }
        }
    }
    close(sockfd);
    close(pipefd[0]);
    close(pipefd[1]);
    delete [] users;
    exit(EXIT_SUCCESS);
}
