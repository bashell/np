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
#include <sys/select.h>
#include "readn_writen_readline.h"

#define ERR_EXIT(m) \
    do { \
        perror(m);\
        exit(EXIT_FAILURE);\
    }while(0)

void str_cli(FILE *fp, int sockfd) {
    int maxfdp1;
    fd_set rset;
    char sendline[MAXLINE], recvline[MAXLINE];

    FD_ZERO(&rset);
    for(;;) {
        FD_SET(fileno(fp), &rset);
        FD_SET(sockfd, &rset);
        maxfdp1 = (sockfd > fileno(fp) ? sockfd : fileno(fp)) + 1;
        if(select(maxfdp1, &rset, NULL, NULL, NULL) == -1)
            ERR_EXIT("select");
        if(FD_ISSET(sockfd, &rset)) {  // socket readable
            if(readline(sockfd, recvline, MAXLINE) == 0)
                fprintf(stderr, "strcli: server terminated prematurely");
            fputs(recvline, stdout);
        }
        if(FD_ISSET(fileno(fp), &rset)) {  // input readable
            if(fgets(sendline, MAXLINE, fp) == NULL)
                return ;
            writen(sockfd, sendline, strlen(sendline));
        }
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
