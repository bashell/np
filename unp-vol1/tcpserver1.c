#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "readn_writen_readline.h"

#define ERR_EXIT(m) \
    do { \
        perror(m);\
        exit(EXIT_FAILURE);\
    }while(0)

void str_echo(int sockfd) {
    ssize_t n;
    char buf[MAXLINE];
again:
    while((n = read(sockfd, buf, MAXLINE)) > 0)
        if(writen(sockfd, buf, n) != n)
            fprintf(stderr, "writen error");
    if(n < 0 && errno == EINTR)
        goto again;
    else if(n < 0)
        fprintf(stderr, "str_echo: read error");
}

int main(int argc, char *argv[])
{
    int listenfd, connfd;
    pid_t childpid;
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

    for(;;) {
        memset(&cliaddr, 0, sizeof(cliaddr));
        cli_len = sizeof(cliaddr);
        connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &cli_len);
        if(connfd == -1)
            ERR_EXIT("accept");
        childpid = fork();
        if(childpid < 0) {
            ERR_EXIT("fork");
        } else if(childpid == 0) {  // child
            close(listenfd);
            str_echo(connfd);
            exit(0);
        }
        close(connfd);
    }
    close(listenfd);
    exit(EXIT_SUCCESS);
}
