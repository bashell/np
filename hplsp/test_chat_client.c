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
#include <poll.h>
#include <fcntl.h>
#include <libgen.h>

#define BUFFER_SIZE 64
#define ERR_EXIT(m) \
    do { \
        perror(m);\
        exit(EXIT_FAILURE);\
    }while(0)

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

    if(connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1)
        ERR_EXIT("connection");

    struct pollfd fds[2];
    fds[0].fd = 0;
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    fds[1].fd = sockfd;
    fds[1].events = POLLIN | POLLRDHUP;
    fds[1].revents = 0;
    
    char read_buf[BUFFER_SIZE];
    int pipefd[2];
    int ret = pipe(pipefd);
    if(ret == -1)
        ERR_EXIT("pipe");

    while(1) {
        ret = poll(fds, 2, -1);
        if(ret == -1) {
            printf("poll failure\n");
            break;
        }
        if(fds[1].revents & POLLRDHUP) {
            printf("server close the connection\n");
            break;
        } else if(fds[1].revents & POLLIN) {
            memset(read_buf, 0, BUFFER_SIZE);
            recv(fds[1].fd, read_buf, BUFFER_SIZE-1, 0);
            printf("%s\n", read_buf);
        }
        if(fds[0].revents & POLLIN) {
            if(splice(0, NULL, pipefd[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE) == -1)
                ERR_EXIT("splice");
            if(splice(pipefd[0], NULL, sockfd, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE) == -1)
                ERR_EXIT("splice");
        }

    }
    close(sockfd);
    exit(EXIT_SUCCESS);
}
