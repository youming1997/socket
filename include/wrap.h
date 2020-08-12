//
// Created by 朱宏宽 on 2020/8/10.
//

#ifndef SOCKET_SERVER_CLIENT_WRAP_H
#define SOCKET_SERVER_CLIENT_WRAP_H

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>

void perror_exit(const char *s);
int Socket(int domain, int type, int protocol);
int Bind(int sockfd, struct sockaddr *sa, socklen_t size);
int Accept(int sockfd, struct sockaddr *sa, socklen_t *size);
int Listen(int sockfd, int num);
int Connect(int sockfd, struct sockaddr *sa, socklen_t size);
int Read(int sockfd, void *s, size_t size);
int Write(int sockfd, const void *s, size_t size);
int Close(int sockfd);

//第三个参数: 应该读取的字节数
int Readn(int sockfd, void *s, size_t size);
int Writen(int sockfd, void *s, size_t size);
//读取缓冲区的一段字符
int ReadByte(int sockfd, char *s);
//readline函数负责读一行的操作, 也就是读到'\n'的时候返回
ssize_t ReadLine(int sockfd, void *s, size_t size);

#endif //SOCKET_SERVER_CLIENT_WRAP_H
