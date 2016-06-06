#define _GNU_SOURCE 1
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
#include <poll.h>
#include <libgen.h>

// 最大用户数
#define USER_LIMIT 5
// 缓冲区大小
#define BUFFER_SIZE 64
// fd限制
#define FD_LIMIT 65535

#define ERR_EXIT(m) \
    do { \
        perror(m);\
        exit(EXIT_FAILURE);\
    }while(0)

// 客户数据
typedef struct tag {
    struct sockaddr_in address;
    char *write_buf;
    char buf[BUFFER_SIZE];
}client_data;

// 设置fd非阻塞
int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
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

    client_data *users = malloc(FD_LIMIT * sizeof(client_data));
    if(!users)
        ERR_EXIT("malloc");
    struct pollfd fds[USER_LIMIT+1];
    int user_counter = 0;  // 当前用户数
    int i;
    for(i = 1; i <= USER_LIMIT; ++i) {
        fds[i].fd = -1;
        fds[i].events = 0;
    }
    fds[0].fd = sockfd;
    fds[0].events = POLLIN | POLLERR;  // 注册可读和错误事件
    fds[0].revents = 0;

    while(1) {
        ret = poll(fds, user_counter+1, -1);
        if(ret == -1) {
            printf("poll failure\n");
            break;
        }
        for(i = 0; i < user_counter+1; ++i) {
            if((fds[i].fd == sockfd) && (fds[i].events == POLLIN)) {  // 连接请求
                struct sockaddr_in client;
                socklen_t client_len = sizeof(client);
                int connfd = accept(sockfd, (struct sockaddr*)&client, &client_len);
                if(connfd == -1) {
                    printf("errno is: %d\n", errno);
                    continue;
                }
                if(user_counter >= USER_LIMIT) {
                    const char *info = "too many users\n";
                    printf("%s", info);
                    send(connfd, info, strlen(info), 0);  // 通知请求客户, 当前客户总数太多
                    close(connfd);
                    continue;
                }
                ++user_counter;
                users[connfd].address = client;  // users[connfd]对应于connfd的客户数据
                setnonblocking(connfd);
                fds[user_counter].fd = connfd;
                fds[user_counter].events = POLLIN | POLLRDHUP | POLLERR;
                fds[user_counter].revents = 0;
                printf("comes a new user, now have %d users\n", user_counter);
            } else if(fds[i].revents & POLLERR) {  // 发生错误事件
                printf("get an error from %d\n", fds[i].fd);
                char errors[100];
                memset(errors, 0, 100);
                socklen_t len = sizeof(errors);
                if(getsockopt(fds[i].fd, SOL_SOCKET, SO_ERROR, &errors, &len) == -1) {
                    printf("get socket option failed\n");
                }
                continue;
            } else if(fds[i].revents & POLLRDHUP) {  // 客户端连接失败
                users[fds[i].fd] = users[fds[user_counter].fd];
                close(fds[i].fd);
                fds[i] = fds[user_counter];
                --i;
                --user_counter;
                printf("a client left\n");
            } else if(fds[i].revents & POLLIN) {  // 发生可读事件
                int connfd = fds[i].fd;
                memset(users[connfd].buf, 0, BUFFER_SIZE);
                ret = recv(connfd, users[connfd].buf, BUFFER_SIZE-1, 0);
                printf("get %d bytes of client data %s from %d\n", ret, users[connfd].buf, connfd);
                if(ret < 0) {
                    if(errno != EAGAIN) {
                        close(connfd);
                        users[fds[i].fd] = users[fds[user_counter].fd];
                        fds[i] = fds[user_counter];
                        --i;
                        --user_counter;
                    }
                } else if(ret == 0) {
                
                } else {
                    int j;
                    for(j = 1; j <= user_counter; ++j) {
                        if(fds[j].fd == connfd)
                            continue;
                        fds[j].events |= ~POLLIN;
                        fds[j].events |= POLLOUT;
                        users[fds[j].fd].write_buf = users[connfd].buf;
                    }
                }
            } else if(fds[i].revents & POLLOUT) {
                int connfd = fds[i].fd;
                if(!users[connfd].write_buf)
                    continue;
                ret = send(connfd, users[connfd].write_buf, strlen(users[connfd].write_buf), 0);
                users[connfd].write_buf = NULL;
                fds[i].events |= ~POLLOUT;
                fds[i].events |= POLLIN;
            }
        }
            
    }
    free(users);
    close(sockfd);
    return 0;
}
