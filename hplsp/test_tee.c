#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>

#define ERR_EXIT(m) \
    do { \
        perror(m);\
        exit(EXIT_FAILURE);\
    }while(0)

int main(int argc, char *argv[])
{
    if(argc != 2) {
        printf("Usage: %s <file>\n", basename(argv[0]));
        exit(EXIT_FAILURE);
    }
    int filefd = open(argv[1], O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if(filefd == -1)
        ERR_EXIT("open");
    int pipefd_stdout[2];
    int ret = pipe(pipefd_stdout);
    if(ret == -1)
        ERR_EXIT("pipe");
    int pipefd_file[2];
    ret = pipe(pipefd_file);
    if(ret == -1)
        ERR_EXIT("pipe");

    if(splice(STDIN_FILENO, NULL, pipefd_stdout[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE) == -1)
        ERR_EXIT("splice");
    if(tee(pipefd_stdout[0], pipefd_file[1], 32768, SPLICE_F_NONBLOCK) == -1)
        ERR_EXIT("tee");
    if(splice(pipefd_file[0], NULL, filefd, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE) == -1)
        ERR_EXIT("splice");
    if(splice(pipefd_stdout[0], NULL, STDOUT_FILENO, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE) == -1)
        ERR_EXIT("splice");
    close(filefd);
    close(pipefd_stdout[0]);
    close(pipefd_stdout[1]);
    close(pipefd_file[0]);
    close(pipefd_file[1]);
    exit(EXIT_SUCCESS);
}
