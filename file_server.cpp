#include <iostream>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <zconf.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#define C_LIMIT 20
#define MAX_INPUT 1024
#define SERV_PORT 6666

typedef struct{
    int connfd;
    sockaddr_in c_addr;
    pthread_t ptid;
    int (* func)(int &fd);
}CONN_INF;

CONN_INF clients[C_LIMIT];

int client_init(int index) {
    clients[index].connfd = -1;
    memset(&clients[index].c_addr, 0, sizeof(sockaddr_in));
    memset(&clients[index].ptid, 0, sizeof(pthread_t));
    clients[index].func = NULL;
}

int recvAndSendFile(int &clntfd) {
    printf("create pthread successful.\n");
    char recvbuf[MAX_INPUT], sendbuf[MAX_INPUT];

    memset(recvbuf, 0, MAX_INPUT);
    memset(sendbuf, 0, MAX_INPUT);

    int len = recv(clntfd, recvbuf, MAX_INPUT - 1, 0);
    if(len == 0) {
        printf("connect has been shutdown.\n");
        memset(recvbuf, 0, MAX_INPUT);
        return -1;
    } else if(-1 == len) {
        perror("recv(): ");
        close(clntfd);
        return 0;
    }

    recvbuf[len] = '\0';
    char file_name[MAX_INPUT];
    strncpy(file_name, recvbuf, strlen(recvbuf));

    FILE *fp = fopen(file_name, "rb");

    struct stat st;

    stat(file_name, &st);

    std::cout << "file has " << st.st_size << " bytes." << std::endl;

    if(NULL == fp) {
        printf("fail to get file named %s\n", recvbuf);
    } else {
        int len = 0;
        while(!feof(fp)) {
            len = fread(sendbuf, sizeof(char), MAX_INPUT, fp);
            if(len != send(clntfd, sendbuf, len, 0)) {
                perror("filesend(): ");
                fclose(fp);
                close(clntfd);
                return 0;
            }
        }
        printf("file %s transfer successful.\n", recvbuf);
        fclose(fp);
    }
    memset(recvbuf, 0, MAX_INPUT);
    close(clntfd);
    return 1;
}



int main() {
    int listenfd, connfd;
    sockaddr_in s_addr, c_addr;
    int ret;

    for (int i = 0; i < C_LIMIT; ++i) {
        client_init(i);
    }

    if(-1 == (listenfd = socket(AF_INET, SOCK_STREAM, 0))) {
        perror("socket(): ");
        return -1;
    }
    printf("socket() ok.\n");

    memset(&s_addr, 0, sizeof(s_addr));
    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    s_addr.sin_port = htons(SERV_PORT);
    size_t size = sizeof(s_addr);

    int on = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    if(-1 == bind(listenfd, (sockaddr *)&s_addr, (socklen_t)size)) {
        perror("bind(): ");
        return -1;
    }
    printf("bind() ok.\n");

    if(-1 == listen(listenfd, C_LIMIT)) {
        perror("listen(): ");
        return -1;
    }
    printf("listen() ok.\n");

    while(true) {
        int number = -1;
        for (int i = 0; i < C_LIMIT; ++i) {
            if(-1 == clients[i].connfd) {
                number = i;
                break;
            }
        }
        if (-1 == number) {
            printf("connections has been overdose.\n");
            continue;
        }
        socklen_t size = sizeof(sockaddr);
        memset(&c_addr, 0, sizeof(c_addr));

        if(-1 == (connfd = accept(listenfd, (sockaddr *)&c_addr, (socklen_t *)&size))) {
            perror("accept(): ");
            continue;
        }
        clients[number].connfd = connfd;
        clients[number].c_addr = c_addr;
        clients[number].func = recvAndSendFile;

        ret = pthread_create(&(clients[number].ptid), NULL, (void *(*)(void *))(clients[number].func), (void *)&clients[number].connfd);
        if (ret) {
            printf("create pthread %d failed.\n", number);
            close(connfd);
            client_init(number);
            continue;
        }

        sleep(1);
    }

    close(listenfd);
    return 0;
}
