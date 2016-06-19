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
#include "readn_writen_readline.h"

#define ERR_EXIT(m) \
    do { \
        perror(m);\
        exit(EXIT_FAILURE);\
    }while(0)

void str_cli(FILE *fp, int sockfd) {
    char sendline[MAXLINE], recvline[MAXLINE];
    while(fgets(sendline, MAXLINE, fp) != NULL) {
        if(writen(sockfd, sendline, strlen(sendline)) != strlen(sendline))
            fprintf(stderr, "writen error");
        if(readline(sockfd, recvline, MAXLINE) == 0)
            ERR_EXIT("str_cli: server terminated prematurel");
        fputs(recvline, stdout);
    }
}

int main(int argc, char *argv[])
{
    int sockfd;
    struct sockaddr_in servaddr;
    if(argc != 2) {
        printf("Usage: %s IP\n", basename(argv[0]));
        exit(EXIT_FAILURE);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1)
        ERR_EXIT("socket");
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8888);
    inet_pton(AF_INET, argv[1], &servaddr.sin_addr);
    if(connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1)
        ERR_EXIT("connect");
    str_cli(stdin, sockfd);

    close(sockfd);
    exit(EXIT_SUCCESS);
}
