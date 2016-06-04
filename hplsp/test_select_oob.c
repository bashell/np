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
#include <libgen.h>

#define ERR_EXIT(m) \
    do { \
        perror(m);\
        exit(EXIT_FAILURE);\
    }while(0)

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

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = inet_addr(ip);

    if(bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1)
        ERR_EXIT("bind");
    if(listen(sockfd, SOMAXCONN) == -1)
        ERR_EXIT("listen");

    struct sockaddr_in client;
    socklen_t client_len = sizeof(client_len);
    int connfd = accept(sockfd, (struct sockaddr*)&client, &client_len);
    if(connfd == -1) 
        ERR_EXIT("accept");

    char buf[1024]; 
    fd_set read_fds, exception_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&exception_fds);
    while(1) {
        memset(buf, 0, sizeof(buf));
        FD_SET(connfd, &read_fds);
        FD_SET(connfd, &exception_fds);
        int ret = select(connfd+1, &read_fds, 0, &exception_fds, NULL);
        if(ret == -1)
            ERR_EXIT("select");
        if(FD_ISSET(connfd, &read_fds)) {
            ret = recv(connfd, buf, sizeof(buf)-1, 0);
            if(ret <= 0)
                break;
            printf("get %d bytes of normal data: %s\n", ret, buf);
        } else if(FD_ISSET(connfd, &exception_fds)) {
            ret = recv(connfd, buf, sizeof(buf)-1, MSG_OOB);
            if(ret <= 0)
                break;
            printf("get %d bytes of oob data: %s\n", ret, buf);
        }
    }
    close(connfd);
    close(sockfd);
    return 0;
}
