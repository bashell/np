#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <libgen.h>

#define USER_LIMIT          5
#define BUFFER_SIZE         1024
#define FD_LIMIT            65535
#define MAX_EVENT_NUMBER    1024
#define PROCESS_LIMIT       65536

#define ERR_EXIT(m) \
    do { \
        perror(m);\
        exit(EXIT_FAILURE);\
    }while(0)

/* 处理客户连接的数据 */
typedef struct tag {
    struct sockaddr_in address;
    int connfd;
    pid_t pid;
    int pipefd[2];
} client_data;

static const char *shm_name = "/my_shm";
int sig_pipefd[2];
int epollfd;
int listenfd;
int shmfd;
char *share_mem = NULL;
client_data *users = NULL;  // 客户连接数组
int *sub_process = NULL;    // 子进程与客户连接的映射表
int user_count = 0;         // 当前客户数量
bool stop_child = false;

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
    int save_errno = errno;
    int msg = sig;
    send(sig_pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}

void addsig(int sig, void (*handler)(int), bool restart = true) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    if(restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    if(sigaction(sig, &sa, NULL) == -1)
        ERR_EXIT("sigaction");
}

void del_resource() {
    close(sig_pipefd[0]);
    close(sig_pipefd[1]);
    close(listenfd);
    close(epollfd);
    shm_unlink(shm_name);
    delete [] users;
    delete [] sub_process;
}

void child_term_handler(int sig) {
    stop_child = true;
}

int run_child(int idx, client_data *users, char *share_mem) {
    struct epoll_event events[MAX_EVENT_NUMBER];
    int child_epollfd = epoll_create(5);
    if(child_epollfd == -1)
        ERR_EXIT("epoll_create");
    int connfd = users[idx].connfd;
    addfd(child_epollfd, connfd);
    int pipefd = users[idx].pipefd[1];
    addfd(child_epollfd, pipefd);
    addsig(SIGTERM, child_term_handler, false);  // 设置信号处理函数

    while(!stop_child) {
        int number = epoll_wait(child_epollfd, events, MAX_EVENT_NUMBER, -1);
        if(number == -1 && errno != EAGAIN)
            ERR_EXIT("epoll_wait");
        for(int i = 0; i < number; ++i) {
            int sockfd = events[i].data.fd;
            if(sockfd == connfd && events[i].events & EPOLLIN) {  // 客户连接有数据到达
                memset(share_mem + idx*BUFFER_SIZE, 0, BUFFER_SIZE);
                int ret = recv(connfd, share_mem + idx*BUFFER_SIZE, BUFFER_SIZE-1, 0);  // 读取客户数据至共享内存中的某一段
                if(ret < 0) {
                    if(errno != EAGAIN)
                        stop_child = true;
                } else if(ret == 0) {
                    stop_child = true;
                } else {
                    send(pipefd, (char*)&idx, sizeof(idx), 0);  // 成功读取客户数据后，使用管道来通知主进程来处理
                }
            } else if(sockfd == pipefd && events[i].events & EPOLLIN) {  // 主进程通过管道将第client个客户的数据发送到本进程负责的客户端
                int client = 0;
                int ret = recv(sockfd, (char*)&client, sizeof(client), 0);  // 接收编号数据
                if(ret < 0) {
                    if(errno != EAGAIN)
                        stop_child = true;
                } else if(ret == 0) {
                    stop_child = true;
                } else {
                    send(connfd, share_mem + client*BUFFER_SIZE, BUFFER_SIZE, 0);
                }
            } else {
                continue;
            }
        }
    }
    close(connfd);
    close(pipefd);
    close(child_epollfd);
    return 0;
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

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);

    if(bind(listenfd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
        ERR_EXIT("bind");
    if(listen(listenfd, SOMAXCONN) == -1)
        ERR_EXIT("listen");

    user_count = 0;
    users = new client_data[USER_LIMIT-1];
    if(users == NULL)
        ERR_EXIT("new");
    sub_process = new int[PROCESS_LIMIT];
    if(sub_process == NULL)
        ERR_EXIT("new");
    for(int i = 0; i < PROCESS_LIMIT; ++i)
        sub_process[i] = -1;

    struct epoll_event events[MAX_EVENT_NUMBER];
    epollfd = epoll_create(5);
    if(epollfd == -1)
        ERR_EXIT("epoll_create");
    addfd(epollfd, listenfd);

    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sig_pipefd);
    if(ret == -1)
        ERR_EXIT("socketpair");
    setnonblocking(sig_pipefd[1]);
    addfd(epollfd, sig_pipefd[0]);

    addsig(SIGCHLD, sig_handler);
    addsig(SIGTERM, sig_handler);
    addsig(SIGINT, sig_handler);
    addsig(SIGPIPE, sig_handler);
    bool stop_server = false;
    bool terminate = false;

    shmfd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if(shmfd == -1)
        ERR_EXIT("shm_open");
    ret = ftruncate(shmfd, USER_LIMIT * BUFFER_SIZE);
    if(ret == -1)
        ERR_EXIT("ftruncate");
    
    share_mem = (char*)mmap(NULL, USER_LIMIT*BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    assert(share_mem != MAP_FAILED);

    while(!stop_server) {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if(number == -1 && errno != EINTR)
            ERR_EXIT("epoll_wait");
        for(int i = 0; i < number; ++i) {
            int sockfd = events[i].data.fd;
            if(sockfd == listenfd) {
                struct sockaddr_in client;
                socklen_t client_len = sizeof(client);
                int connfd = accept(listenfd, (struct sockaddr*)&client, &client_len);
                if(connfd == -1)
                    ERR_EXIT("accept");
                if(user_count >= USER_LIMIT) {
                    const char *info = "too many users\n";
                    printf("%s", info);
                    send(connfd, info, strlen(info), 0);
                    close(connfd);
                    continue;
                }
                users[user_count].address = client;
                users[user_count].connfd = connfd;
                ret = socketpair(AF_UNIX, SOCK_STREAM, 0, users[user_count].pipefd);
                if(ret == -1)
                    ERR_EXIT("socketpair");
                pid_t pid = fork();
                if(pid < 0) {
                    close(connfd);
                    continue;
                } else if(pid == 0) {  // child
                    close(epollfd);
                    close(listenfd);
                    close(users[user_count].pipefd[0]);
                    close(sig_pipefd[0]);
                    close(sig_pipefd[1]);
                    run_child(user_count, users, share_mem);
                    munmap((void*)share_mem, USER_LIMIT*BUFFER_SIZE);
                    exit(0);
                } else {  // parend
                    close(connfd);
                    close(users[user_count].pipefd[1]);
                    addfd(epollfd, users[user_count].pipefd[0]);
                    users[user_count].pid = pid;
                    sub_process[pid] = user_count;
                    ++user_count;
                }
            } else if(sockfd == sig_pipefd[0] && events[i].events & EPOLLIN) {
                int sig;
                char signals[1024];
                ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if(ret == -1)
                    continue;
                else if(ret == 0)
                    continue;
                else {
                    for(int i = 0; i < ret; ++i) {
                        switch(signals[i]) {
                            case SIGCHLD:
                                pid_t pid;
                                int stat;
                                while((pid == waitpid(-1, &stat, WNOHANG)) > 0) {
                                    int del_user = sub_process[pid];
                                    sub_process[pid] = -1;
                                    if(del_user < 0 || del_user > USER_LIMIT)
                                        continue;
                                    if(epoll_ctl(epollfd, EPOLL_CTL_DEL, users[del_user].pipefd[0], 0) == -1)
                                        ERR_EXIT("epoll_ctl");
                                    close(users[del_user].pipefd[0]);
                                    users[del_user] = users[--user_count];
                                    sub_process[users[del_user].pid] = del_user;
                                }
                                if(terminate && user_count == 0)
                                    stop_server = true;
                                break;
                            case SIGTERM:
                            case SIGINT:
                                printf("kill all children now\n");
                                if(user_count == 0) {
                                    stop_server = true;
                                    break;
                                }
                                for(int i = 0; i < user_count; ++i) {
                                    int pid = users[i].pid;
                                    kill(pid, SIGTERM);
                                }
                                terminate = true;
                                break;
                            default:
                                break;
                        }
                    }
                }
            } else if(events[i].events & EPOLLIN) {
                int child = 0;
                ret = recv(sockfd, (char*)&child, sizeof(child), 0);
                printf("read data from child accross pipe\n");
                if(ret == -1)
                    continue;
                else if(ret == 0)
                    continue;
                else {
                    for(int j = 0; j < user_count; ++j) {
                        if(users[j].pipefd[0] != sockfd) {
                            printf("send data to child accross pipe\n");
                            send(users[j].pipefd[0], (char*)&child, sizeof(child), 0);
                        }
                    }
                }
            }
        }
    }
    del_resource();
    exit(EXIT_SUCCESS);
}
