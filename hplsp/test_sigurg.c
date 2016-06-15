#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <libgen.h>
#include <signal.h>
#include <fcntl.h>

#define ERR_EXIT(m) \
    do { \
        perror(m);\
        exit(EXIT_FAILURE);\
    }while(0)

#define BUF_SIZE 1024

static int connfd;

void sig_urg(int sig) {
    int save_errno = errno;
    char buffer[BUF_SIZE];
    memset(buffer, 0, BUF_SIZE);
    int ret = recv(connfd, buffer, BUF_SIZE-1, MSG_OOB);
    printf("get %d bytes of oob data '%s'\n", ret, buffer);
    errno = save_errno;
}

void addsig(int sig, void (*sig_handler)(int)) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    if(sigaction(sig, &sa, NULL) == -1)
        ERR_EXIT("sigaction");
}

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

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);

    if(bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
        ERR_EXIT("bind");
    if(listen(sockfd, SOMAXCONN) == -1)
        ERR_EXIT("listen");

    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);
    connfd = accept(sockfd, (struct sockaddr*)&client, &client_len);
    if(connfd == -1)
        ERR_EXIT("accept");
    else {
        addsig(SIGURG, sig_urg);
        fcntl(connfd, F_SETOWN, getpid());
        char buffer[BUF_SIZE];
        while(1) {
            memset(buffer, 0, BUF_SIZE);
            int ret = recv(connfd, buffer, BUF_SIZE-1, 0);
            if(ret <= 0)
                break;
            printf("get %d bytes of normal data '%s'\n", ret, buffer);
        }
        close(connfd);
    }
    close(sockfd);
    exit(EXIT_SUCCESS);
}
