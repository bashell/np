#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include "readn_writen_readline.h"

#define ERR_EXIT(m) \
    do { \
        perror(m);\
        exit(EXIT_FAILURE);\
    }while(0)

int main(int argc, char *argv[])
{
    int i, maxi;
    int listenfd, connfd, maxfd, sockfd;
    int nready, client[FD_SETSIZE];
    ssize_t n;
    char buf[MAXLINE];
    fd_set rset, allset;
    socklen_t cli_len;
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

    maxfd = listenfd;
    maxi = -1;
    for(i = 0; i < FD_SETSIZE; ++i)
        client[i] = -1;
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);  // 注册监听描述符

    for(;;) {
        rset = allset;
        nready = select(maxfd+1, &rset, NULL, NULL, NULL);
        if(nready == -1)
            ERR_EXIT("select");

        if(FD_ISSET(listenfd, &rset)) {  // 新客户连接
            memset(&cliaddr, 0, sizeof(cliaddr));
            cli_len = sizeof(cliaddr);
            connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &cli_len);
            if(connfd == -1)
                ERR_EXIT("accept");
            for(i = 0; i < FD_SETSIZE; ++i) {
                if(client[i] < 0) {  // 从客户数组中找到第一个可用位置，标记已连接描述符
                    client[i] = connfd;
                    break;
                }
            }
            if(i == FD_SETSIZE)
                ERR_EXIT("too many clients");
            FD_SET(connfd, &allset);  // 添加至描述符集中
            if(connfd > maxfd)
                maxfd = connfd;  // 更新maxfd
            if(i > maxi)  // 客户数组中所有已用位置的最大下标
                maxi = i;
            if(--nready <= 0)  // 所有可读描述符都已处理
                continue;
        }

        for(i = 0; i < maxi; ++i) {  // 轮询检测
            if((sockfd = client[i]) < 0)
                continue;
            if(FD_ISSET(sockfd, &rset)) {
                if((n = read(sockfd, buf, MAXLINE)) == 0) {  // 对方终止连接
                    close(sockfd);
                    FD_CLR(sockfd, &allset);
                    client[i] = -1;
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
