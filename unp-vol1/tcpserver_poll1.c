#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/poll.h>
#include "readn_writen_readline.h"

#define OPEN_MAX 512

#define ERR_EXIT(m) \
    do { \
        perror(m);\
        exit(EXIT_FAILURE);\
    }while(0)

int main(int argc, char *argv[])
{
    int i, maxi;
    int listenfd, connfd, sockfd;
    int nready;
    ssize_t n;
    char buf[MAXLINE];
    socklen_t cli_len;
    struct pollfd client[OPEN_MAX];
    struct sockaddr_in servaddr, cliaddr;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd == -1)
        ERR_EXIT("socket");

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8888);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1)
        ERR_EXIT("bind");
    if(listen(listenfd, SOMAXCONN) == -1)
        ERR_EXIT("listen");

    client[0].fd = listenfd;
    client[0].events = POLLRDNORM;
    for(i = 1; i < OPEN_MAX; ++i)
        client[i].fd = -1;
    maxi = 0;  // client数组中已使用的最大下标
    
    for(;;) {
        nready = poll(client, maxi+1, -1);
        if(nready == -1)
            ERR_EXIT("poll");
        if(client[0].revents & POLLRDNORM) {  // 新客户连接
            memset(&cliaddr, 0, sizeof(cliaddr));
            cli_len = sizeof(cliaddr);
            connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &cli_len);
            if(connfd == -1)
                ERR_EXIT("accept");
            for(i = 1; i < OPEN_MAX; ++i) {
                if(client[i].fd < 0) {
                    client[i].fd = connfd;
                    break;
                }
            }
            if(i == OPEN_MAX)
                fprintf(stderr, "too many clients!\n");
            client[i].revents = POLLRDNORM;
            if(i > maxi)
                maxi = i;
            if(--nready <= 0)
                continue;
        }
        for(i = 1; i <= maxi; ++i) {
            if((sockfd = client[i].fd) < 0)
                continue;
            if(client[i].revents & (POLLRDNORM | POLLERR)) {
                if((n = read(sockfd, buf, MAXLINE)) < 0) {
                    if(errno == ECONNRESET) {
                        close(sockfd);
                        client[i].fd = -1;
                    } else {
                        fprintf(stderr, "read error");
                    }
                } else if(n == 0) {
                    close(sockfd);
                    client[i].fd = -1;
                } else {
                    writen(sockfd, buf, n);
                }
                if(--nready <= 0)
                    break;
            }
        }
    }
    close(listenfd);
    exit(EXIT_SUCCESS);
}
