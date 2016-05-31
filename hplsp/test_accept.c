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

int main(int argc, char *argv[])
{
    if(argc <= 2) {
        fprintf(stderr, "Usage: IP PORT\n");
        exit(EXIT_FAILURE);
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);

    struct sockaddr_in myaddr;
    bzero(&myaddr, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_port = htons(port);
    myaddr.sin_addr.s_addr = inet_addr(ip);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1)
        ERR_EXIT("socket");
    if(bind(sockfd, (struct sockaddr*)&myaddr, sizeof(myaddr)) == -1)
        ERR_EXIT("bind");
    if(listen(sockfd, SOMAXCONN) == -1)
        ERR_EXIT("listen");

    sleep(20);

    struct sockaddr_in client;
    socklen_t client_addrlen = sizeof(client);
    int connfd = accept(sockfd, (struct sockaddr*)&client, &client_addrlen);
    if(connfd == -1)
        ERR_EXIT("accept");
    else {
        char remote[INET_ADDRSTRLEN];
        printf("connected with ip: %s and port: %d\n", inet_ntop(AF_INET, &client.sin_addr, remote, INET_ADDRSTRLEN), ntohs(client.sin_port));
        close(connfd);
    }
    close(sockfd);
    return 0;
}
