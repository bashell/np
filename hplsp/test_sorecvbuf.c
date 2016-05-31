#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define ERR_EXIT(m) \
    do { \
        perror(m);\
        exit(EXIT_FAILURE);\
    }while(0)

#define BUF_SIZE 1024

int main(int argc, char *argv[])
{
    if(argc <= 2) {
        printf("Usage: %s IP PORT RECV_BUFFER_SIZE\n", basename(argv[0]));
        exit(EXIT_FAILURE);
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1)
        ERR_EXIT("socket");

    int reuse = 1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1)
        ERR_EXIT("setsockopt");

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);

    int recvbuf = atoi(argv[3]);
    int len = sizeof(recvbuf);

    if(setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &recvbuf, sizeof(recvbuf)) == -1)
        ERR_EXIT("setsockopt");
    if(getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &recvbuf, (socklen_t*)&len) == -1)
        ERR_EXIT("getsockopt");

    if(bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
        ERR_EXIT("bind");

    if(listen(sockfd, SOMAXCONN) == -1)
        ERR_EXIT("listen");

    struct sockaddr_in client;
    memset(&client, 0, sizeof(client));
    socklen_t client_addrlen = sizeof(client);
    int connfd = accept(sockfd, (struct sockaddr*)&client, &client_addrlen);
    if(connfd == -1)
        ERR_EXIT("accept");
    else {
        char buffer[BUF_SIZE];
        memset(buffer, 0, BUF_SIZE);
        while(recv(connfd, buffer, BUF_SIZE-1, 0) > 0) {}
        close(connfd);
    }
    close(sockfd);
    return 0;
}
