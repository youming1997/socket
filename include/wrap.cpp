//
// Created by 朱宏宽 on 2020/8/10.
//

#include "wrap.h"

void perror_exit(const char *s) {
    perror(s);
    exit(-1);
}

int Socket(int domain, int type, int protocol) {
    int n;
    if((n = socket(domain, type, protocol)) < 0) {
        perror_exit("socket error");
    }

    return n;
}

int Bind(int sockfd, struct sockaddr *sa, socklen_t size = sizeof(struct sockaddr)) {
    int n;
    if((n = bind(sockfd, sa, size)) < 0){
        perror_exit("bind error");
    }

    return n;
}

int Accept(int sockfd, struct sockaddr *sa, socklen_t *size) {
    int n;
    while((n = accept(sockfd, sa, size)) < 0) {
        if(errno == EINTR || errno == ECONNABORTED)
            continue;
        else
            perror_exit("accept error");
    }

    return n;
}

int Listen(int sockfd, int num = 10) {
    int n;
    if((n = listen(sockfd, num)) < 0) {
        perror_exit("listen error");
    }

    return n;
}

int Connect(int sockfd, struct sockaddr *sa, socklen_t size) {
    int n;
    if((n = connect(sockfd, sa, size)) < 0) {
        perror_exit("connect error");
    }

    return n;
}

int Read(int sockfd, void *s, size_t size) {
    int n;
    while((n = read(sockfd, s, size)) == -1) {
        if(errno == EINTR)
            continue;
        else
            return -1;
    }

    return n;
}


int Write(int sockfd, const void *s, size_t size) {
    int n;
    while((n = write(sockfd, s, size)) == -1) {
        if(errno == EINTR)
            continue;
        else
            return -1;
    }

    return n;
}

int Close(int sockfd) {
    int n;
    if((n = close(sockfd)) == -1) {
        perror_exit("close error");
    }

    return n;
}

//第三个参数: 应该读取的字节数
int Readn(int sockfd, void *s, size_t size) {
    size_t size_left = size;
    ssize_t size_read;
    char *str;

    str = (char *)(s);

    while(size_left > 0){
        if((size_read = read(sockfd, str, size_left)) == -1) {
            if(errno == EINTR)
                size_read = 0;
            else
                return -1;
        } else if(size_read == 0){
            break;
        }
        size_left -= size_read;
        str += size_read;
    }
}

int Writen(int sockfd, void *s, size_t size) {
    size_t size_left = size;
    ssize_t size_write;
    char *str;

    str = (char *)(s);

    while(size_left > 0){
        if((size_write = write(sockfd, str, size_left)) == -1) {
            if(errno == EINTR)
                size_write = 0;
            else
                return -1;
        } else if(size_write == 0){
            break;
        }
        size_left -= size_write;
        str += size_write;
    }
}

//读取缓冲区的一段字符
int ReadByte(int sockfd, char *s) {
    static int read_count;
    static char *ptr;
    static char buf[100];

    if(read_count <= 0) {
        while((read_count = read(sockfd, buf, sizeof(buf))) < 0) {
            if(errno == EINTR)
                continue;
            else
                return -1;
        }
        if(read_count == 0)
            return 0;
        ptr = buf;
    }
    read_count--;
    *s = *ptr++;

    return 1;
}

//readline函数负责读一行的操作, 也就是读到'\n'的时候返回
ssize_t ReadLine(int sockfd, void *s, size_t size) {
    char *str;
    char c;
    int n;
    ssize_t i;

    str = (char *)s;
    for (i = 1; i < size; ++i) {
        if(1 == (n = ReadByte(sockfd, &c))) {
            *str = c;
            if(c == '\n')
                break;
        } else if(n == 0) {
            *str = 0;
            return i - 1;
        } else
            return -1;
    }
    *str = 0;

    return i;
}