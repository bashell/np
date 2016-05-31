#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>

#define ERR_EXIT(m) \
    do { \
        perror(m);\
        exit(EXIT_FAILURE);\
    }while(0)

#define BUF_SIZE 1024
static const char *status_line[2] = {"200 OK", "500 Internal server error"};

int main(int argc, char *argv[]) 
{
    if(argc <= 3) {
        printf("Usage: %s IP PORT filename\n", basename(argv[0]));
        exit(EXIT_FAILURE);
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    const char *file_name = argv[3];

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
    socklen_t clientLen = sizeof(client);
    int connfd = accept(sockfd, (struct sockaddr*)&client, &clientLen);
    if(connfd == -1)
        ERR_EXIT("accept");
    else {
        char header_buf[BUF_SIZE];  // HTTP应答报文缓存区
        memset(header_buf, 0, BUF_SIZE);
        char *file_buf = NULL;  // 文件缓存
        struct stat file_stat;  // 获取文件属性
        bool valid = true;      // 记录文件是否有效
        int len = 0;
        if(stat(file_name, &file_stat) < 0) {  // 文件不存在
            valid = false;
        } else {
            if(S_ISDIR(file_stat.st_mode)) {  // 是一个目录
                valid = false;
            } else if(file_stat.st_mode & S_IROTH) {  // 用户有权限读取该文件
                int fd = open(file_name, O_RDONLY);
                file_buf = new char[file_stat.st_size+1];
                memset(file_buf, 0, file_stat.st_size+1);
                if(read(fd, file_buf, file_stat.st_size) < 0)
                    valid = false;
            } else {
                valid = false;
            }
        }
        // 发送HTTP应答报文
        if(valid) {  
            int ret = snprintf(header_buf, BUF_SIZE-1, "%s %s\r\n", "HTTP/1.1", status_line[0]);
            len += ret;
            ret = snprintf(header_buf + len, BUF_SIZE-1-len, "Content-Length: %ld\r\n", file_stat.st_size);
            len += ret;
            ret = snprintf(header_buf + len, BUF_SIZE-1-len, "%s", "\r\n");
            // writev将header_buf和file_buf集中写出
            struct iovec iv[2];
            iv[0].iov_base = header_buf;
            iv[0].iov_len = strlen(header_buf);
            iv[1].iov_base = file_buf;
            iv[1].iov_len = file_stat.st_size;
            ret = writev(connfd, iv, 2);
        } else {  // 文件无效时， 通知客户端发生内部错误
            int ret = snprintf(header_buf, BUF_SIZE-1, "%s %s\r\n", "HTTP/1.1", status_line[1]);
            len += ret;
            ret = snprintf(header_buf + len, BUF_SIZE-1-len, "%s", "\r\n");
            send(connfd, header_buf, strlen(header_buf), 0);
        }
        close(connfd);
        delete[] file_buf;
    }
    close(sockfd);
    return 0;
}
