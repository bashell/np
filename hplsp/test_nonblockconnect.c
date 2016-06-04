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
#include <sys/select.h>

#define ERR_EXIT(m) \
    do { \
        perror(m);\
        exit(EXIT_FAILURE);\
    }while(0)

#define BUFFER_SIZE 1024

int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

int unblock_connect(const char *ip, int port, int time) {
    int ret = 0;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1)
        ERR_EXIT("socket");
    int fdopt = setnonblocking(sockfd);
    ret = connect(sockfd, (struct sockaddr*)&addr, sizeof(addr));
    if(ret == 0) {
        printf("connect with server immediately\n");
        fcntl(sockfd, F_SETFL, fdopt);
        return sockfd;
    } else if(errno != EINPROGRESS) {
        printf("unblock connect not support\n");
        exit(EXIT_FAILURE);
    }

    fd_set readfds, writefds;
    struct timeval timeout;
    FD_ZERO(&readfds);
    FD_SET(sockfd, &writefds);

    timeout.tv_sec = time;
    timeout.tv_usec = 0;
    ret = select(sockfd+1, NULL, &writefds, NULL, &timeout);
    if(ret <= 0) {
        printf("connection time out\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    if(!FD_ISSET(sockfd, &writefds)) {
        printf("no events on sockfd found\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    int error = 0;
    socklen_t len = sizeof(error);
    if(getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) == -1) {
        printf("get socket option failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    if(error != 0) {
        printf("connection failed after select with the error: %d\n", error);
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("connection ready after select with the socket: %d\n", sockfd);
    fcntl(sockfd, F_SETFL, fdopt);
    return sockfd;
}

int main(int argc, char *argv[])
{
    if(argc <= 2) {
        printf("Usage: %s IP PORT\n", basename(argv[0]));
        exit(EXIT_FAILURE);
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);

    int sockfd = unblock_connect(ip, port, 10);
    if(sockfd < 0)
        exit(EXIT_FAILURE);
    close(sockfd);
    exit(EXIT_SUCCESS);
}
