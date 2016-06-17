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
#include <sys/epoll.h>
#include <fcntl.h>

#define ERR_EXIT(m) \
    do { \
        perror(m);\
        exit(EXIT_FAILURE);\
    }while(0)

static const char * request = "GET http://localhost/index.html HTTP/1.1\r\nConnection: keep-alive\r\n\r\nxxxxxxxxxxxx";

int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epoll_fd, int fd) {
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = EPOLLOUT | EPOLLET | EPOLLERR;
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1)
        ERR_EXIT("epoll_ctl");
    setnonblocking(fd);
}

/* 向服务器写入len字节的数据 */
bool write_nbytes(int sockfd, const char *buffer, int len) {
    int bytes_write = 0;
    printf("write out %d bytes to socket %d\n", len, sockfd);
    for(;;) {
        bytes_write = send(sockfd, buffer, len, 0);
        if(bytes_write == -1)
            return false;
        else if(bytes_write == 0)
            return false;
        len -= bytes_write;
        buffer = buffer + bytes_write;
        if(len <= 0)
            return true;
    }
}

/* 从服务器读取数据 */
bool read_once(int sockfd, char *buffer, int len) {
    int bytes_read = 0;
    memset(buffer, 0, len);
    bytes_read = recv(sockfd, buffer, len, 0);
    if(bytes_read == -1)
        return false;
    else if(bytes_read == 0)
        return false;
    printf("read in %d bytes from socket %d with content: %s\n", bytes_read, sockfd, buffer);
    return true;
}

/* 向服务器发起num个TCP连接，调整num进行压力测试 */
void start_conn(int epoll_fd, int num, const char * ip, int port) {
    int ret = 0;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);

    for(int i = 0; i < num; ++i) {
        sleep(1);
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if(sockfd == -1)
            ERR_EXIT("socket");
        printf("create 1 socket\n");
        if(connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
            ERR_EXIT("connect");
        printf("build connection %d\n", i);
        addfd(epoll_fd, sockfd);
    }
}

void close_conn(int epoll_fd, int sockfd) {
    if(epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sockfd, 0) == -1)
        ERR_EXIT("epoll_ctl");
    close(sockfd);
}

int main(int argc, char *argv[]) 
{
    assert(argc == 4);
    int epoll_fd = epoll_create(100);
    if(epoll_fd == -1)
        ERR_EXIT("epoll_create");
    start_conn(epoll_fd, atoi(argv[3]), argv[1], atoi(argv[2]));
    struct epoll_event events[10000];
    char buffer[2048];
    for(;;) {
        int fds = epoll_wait(epoll_fd, events, 10000, 2000);
        if(fds == -1)
            ERR_EXIT("epoll_wait");
        for(int i = 0; i < fds; ++i) {
            int sockfd = events[i].data.fd;
            if(events[i].events & EPOLLIN) {
                if(!read_once(sockfd, buffer, 2048))
                    close_conn(epoll_fd, sockfd);
                struct epoll_event event;
                event.events = EPOLLOUT | EPOLLET | EPOLLERR;
                event.data.fd = sockfd;
                if(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sockfd, &event) == -1)
                    ERR_EXIT("epoll_ctl");
            } else if(events[i].events & EPOLLOUT) {
                if(!write_nbytes(sockfd, request, strlen(request)))
                    close_conn(epoll_fd, sockfd);
                struct epoll_event event;
                event.events = EPOLLIN | EPOLLET | EPOLLERR;
                event.data.fd = sockfd;
                if(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sockfd, &event) == -1)
                    ERR_EXIT("epoll_ctl");
            } else if(events[i].events & EPOLLERR) {
                close_conn(epoll_fd, sockfd);
            }
        }
    }
    exit(EXIT_SUCCESS);
}

