#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/msg.h>

#define ERR_EXIT(m) \
    do { \
        perror(m);\
        exit(EXIT_FAILURE);\
    }while(0)

static const int CONTROL_LEN = CMSG_LEN(sizeof(int));

void send_fd(int fd, int fd_to_send) {
    struct iovec iov[1];
    struct msghdr msg;
    char buf[0];
    iov[0].iov_base = buf;
    iov[0].iov_len = 1;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    struct cmsghdr cm;
    cm.cmsg_len = CONTROL_LEN;
    cm.cmsg_level = SOL_SOCKET;
    cm.cmsg_type = SCM_RIGHTS;
    *(int*)CMSG_DATA(&cm) = fd_to_send;
    msg.msg_control = &cm;
    msg.msg_controllen = CONTROL_LEN;
    sendmsg(fd, &msg, 0);
}

int recv_fd(int fd) {
    struct iovec iov[1];
    struct msghdr msg;
    char buf[0];
    iov[0].iov_base = buf;
    iov[0].iov_len = 1;
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    struct cmsghdr cm;
    msg.msg_control = &cm;
    msg.msg_controllen = CONTROL_LEN;
    recvmsg(fd, &msg, 0);
    int fd_to_read = *(int*)CMSG_DATA(&cm);
    return fd_to_read;
}

int main()
{
    int pipefd[2];
    int fd_to_pass = 0;
    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, pipefd);
    if(ret == -1)
        ERR_EXIT("socketpair");
    pid_t pid = fork();
    if(pid < 0)
        ERR_EXIT("fork");
    if(pid == 0) {  // child
        close(pipefd[0]);
        fd_to_pass = open("test.txt", O_RDWR, 0666);
        send_fd(pipefd[1], (fd_to_pass > 0) ? fd_to_pass : 0);
        close(fd_to_pass);
        exit(0);
    }
    // parent
    close(pipefd[1]);
    fd_to_pass = recv_fd(pipefd[0]);
    char buf[1024];
    memset(buf, 0, 1024);
    read(fd_to_pass, buf, 1024);
    printf("get fd %d and data %s\n", fd_to_pass, buf);
    close(fd_to_pass);
    exit(1);
}
