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

#define ERR_EXIT(m) \
    do { \
        perror(m);\
        exit(EXIT_FAILURE);\
    }while(0)

int timeout_connect(const char *ip, int port, int time) {
    int ret = 0;
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1)
        ERR_EXIT("socket");
    struct timeval timeout;
    timeout.tv_sec = time;
    timeout.tv_usec = 0;
    socklen_t time_len = sizeof(time);
    ret = setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, time_len);
    if(ret == -1)
        ERR_EXIT("setsockopt");
    ret = connect(sockfd, (struct sockaddr*)&addr, sizeof(addr));
    if(ret == -1) {
        if(errno == EINPROGRESS) {
            printf("Connecting timeout, process timeout logit\n");
            return -1;
        }
        printf("error occur when connecting to server\n");
        return -1;
    }
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

    int sockfd = timeout_connect(ip, port, 10);
    if(sockfd < 0)
        exit(EXIT_FAILURE);
    exit(EXIT_SUCCESS);
}
