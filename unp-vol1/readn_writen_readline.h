#ifndef _READN_WRITEN_READLINE_H_
#define _READN_WRITEN_READLINE_H_

#include <unistd.h>
#include <errno.h>

#define MAXLINE 1024

static int read_cnt;
static char *read_ptr;
static char read_buf[MAXLINE];

/******************** Readn ********************/
ssize_t readn(int fd, void *vptr, size_t n) {
    size_t nleft;
    ssize_t nread;
    char *ptr;

    ptr = vptr;
    nleft = n;
    while(nleft > 0) {
        if((nread = read(fd, ptr, nleft)) < 0) {
            if(errno == EINTR)
                nread = 0;
            else
                return -1;  // ERROR
        } else if(nread == 0) {  // EOF
            break;
        }
        nleft -= nread;
        ptr += nread;
    }
    return n - nleft;
}

/******************** Writen ********************/
ssize_t writen(int fd, void *vptr, size_t n) {
    size_t nleft;
    ssize_t nwritten;
    const char *ptr;

    ptr = vptr;
    nleft = n;
    while(nleft > 0) {
        if((nwritten = write(fd, ptr, nleft)) <= 0) {
            if(nwritten < 0 && errno == EINTR)
                nwritten = 0;
            else
                return -1;  // ERROR
        }
        nleft -= nwritten;
        ptr += nwritten;
    }
    return n;
}

/******************** Readline ********************/
static ssize_t my_read(int fd, char *ptr) {
    if(read_cnt <= 0) {
again:
        if((read_cnt = read(fd, read_buf, sizeof(read_buf))) < 0) {
            if(errno == EINTR)
                goto again;
            return -1;
        } else if(read_cnt == 0) {
            return 0;
        }
        read_ptr = read_buf;
    }
    --read_cnt;
    *ptr = *read_ptr++;
    return 1;
}

ssize_t readline(int fd, void *vptr, size_t maxlen) {
    ssize_t n, rc;
    char c, *ptr;

    ptr = vptr;
    for(n = 1; n < maxlen; ++n) {
        if((rc = my_read(fd, &c)) == 1) {
            *ptr++ = c;
            if(c == '\n')  // newline
                break;
        } else if(rc == 0) {  // EOF
            *ptr = 0;
            return n-1;
        } else {  // ERROR
            return -1;
        }
    }
    *ptr = 0;
    return n;
}

ssize_t readlinebuf(void **vptrptr) {
    if(read_cnt)
        *vptrptr = read_ptr;
    return read_cnt;
}

#endif  /* _READN_WRITEN_READLINE_H_ */
