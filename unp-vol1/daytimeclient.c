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

#define MAXLINE 1024

#define ERR_EXIT(m) \
    do { \
        perror(m);\
        exit(EXIT_FAILURE);\
    }while(0)

int main(int argc, char *argv[])
{
    if(argc != 2) {
        printf("Usage: %s IP\n", basename(argv[0]));
        exit(EXIT_FAILURE);
    }

    int n;
    char recvline[MAXLINE + 1];
    const char *ip = argv[1];
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1)
        ERR_EXIT("socket");
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(13);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    
    if(connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
        ERR_EXIT("connect");
    while((n = read(sockfd, recvline, MAXLINE)) > 0) {
        recvline[n] = 0;
        if(fputs(recvline, stdout) == EOF)
            fprintf(stderr, "fputs error\n");
    }
    if(n < 0)
        fprintf(stderr, "read error\n");
    exit(EXIT_SUCCESS);
}
