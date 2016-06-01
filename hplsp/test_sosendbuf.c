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

#define ERR_EXIT(m) \
    do { \
        perror(m);\
        exit(EXIT_FAILURE);\
    }while(0)

#define BUF_SIZE 512

int main(int argc, char *argv[]) 
{
    if(argc <= 3) {
        printf("Usage: %s IP PORT SEND_BUFFER_SIZE\n", basename(argv[0]));
        exit(EXIT_FAILURE);
    }   
    const char *ip = argv[1];
    int port = atoi(argv[2]);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1) 
        ERR_EXIT("socket");

    int reuse = 1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1)
        ERR_EXIT("setsocketopt");

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);

    int sendbuf = atoi(argv[3]);
    int len = sizeof(sendbuf);
    if(setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &sendbuf, sizeof(sendbuf)) == -1)
        ERR_EXIT("setsocketopt");
    if(getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &sendbuf, (socklen_t*)&len) == -1)
        ERR_EXIT("getsocketopt");
    printf("The tcp send buffer size after setting is %d\n", sendbuf);

    if(connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        char buffer[BUF_SIZE];
        memset(buffer, 0, BUF_SIZE);
        send(sockfd, buffer, BUF_SIZE, 0);
    }
    close(sockfd);
    return 0;
}
