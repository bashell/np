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
#include <sys/select.h>

#define MAXLINE 1024
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
        write(sockfd, buf, n);
    if(n < 0 && errno == EINTR)
        goto again;
    else if(n < 0)
        ERR_EXIT("str_echo: read error");
}

void sig_chld(int signo) {
    pid_t pid;
    int stat;
    while((pid = waitpid(-1, &stat, WNOHANG)) > 0)
        printf("child %d terminated\n", pid);
    return ;
}

int main(int argc, char *argv[])
{
    int listenfd, connfd, udpfd, nready, maxfdp1;
    char mesg[MAXLINE];
    pid_t childpid;
    fd_set rset;
    ssize_t n;
    socklen_t len;
    const int on = 1;
    struct sockaddr_in servaddr, cliaddr;

    /* Tcp socket */
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd == -1)
        ERR_EXIT("socket");
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8888);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1)
        ERR_EXIT("setsockopt");
    if(bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1)
        ERR_EXIT("bind");
    if(listen(listenfd, SOMAXCONN) == -1)
        ERR_EXIT("listen");

    /* Udp socket */
    udpfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(udpfd == -1)
        ERR_EXIT("socket");
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8888);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(udpfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1)
        ERR_EXIT("bind");

    signal(SIGCHLD, sig_chld);

    FD_ZERO(&rset);
    maxfdp1 = (listenfd > udpfd ? listenfd : udpfd) + 1;
    for(;;) {
        FD_SET(listenfd, &rset);
        FD_SET(udpfd, &rset);
        if((nready = select(maxfdp1, &rset, NULL, NULL, NULL)) < 0) {
            if(errno == EINTR)
                continue;
            else
                ERR_EXIT("select");
        }
        if(FD_ISSET(listenfd, &rset)) {
            memset(&cliaddr, 0, sizeof(cliaddr));
            len = sizeof(cliaddr);
            connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &len);
            if(connfd == -1)
                ERR_EXIT("accept");
            if((childpid = fork()) < 0) {
                ERR_EXIT("fork");
            } else if(childpid == 0) {  // child
                close(listenfd);
                str_echo(connfd);
                exit(0);
            }
            close(connfd);  // parent
        }
        if(FD_ISSET(udpfd, &rset)) {
            memset(&cliaddr, 0, sizeof(cliaddr));
            len = sizeof(cliaddr);
            n = recvfrom(udpfd, mesg, MAXLINE, 0, (struct sockaddr*)&cliaddr, &len);
            sendto(udpfd, mesg, n, 0, (struct sockaddr*)&cliaddr, len);
        }
    }
    close(listenfd);
    close(udpfd);
    exit(EXIT_SUCCESS);
}   
