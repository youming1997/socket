#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

#include "include/wrap.h"

#define LINE_MAX 1024

//event中的函数模型
//void (*event_callback_fn)(evutil_socket_t, short, void *)
void signal_cb(evutil_socket_t sockfd, short event, void *arg) {
    struct timeval tv = {2, 0};
    struct event_base *base = (struct event_base *)arg;

    printf("Caught a signal, will be clean in 2 seconds...\n");
    event_base_loopexit(base, &tv);
}

void read_cb(evutil_socket_t clientfd, short event, void * arg) {
    char buf[LINE_MAX];
    int recvlen = Read(clientfd, buf, LINE_MAX);
    if(-1 == recvlen) {
        perror_exit("read error");
    } else if(recvlen == 0) {
        printf("Connection as been shutdown.\n");
        close(clientfd);
        return ;
    } else {
        buf[recvlen] = '\0';
        printf("client %d said: %s", clientfd, buf);
        int sendlen = Write(clientfd, buf, recvlen);
        if(-1 == sendlen) {
            perror_exit("send error");
        }
    }
}

void accept_cb(evutil_socket_t serverfd, short event, void *arg) {
    int connfd;
    struct sockaddr_in c_addr;
    socklen_t socklen = sizeof(struct sockaddr_in);

    connfd = Accept(serverfd, (struct sockaddr *)&c_addr, &socklen);

    struct event_base *base = (struct event_base *)arg;

    char message[] = "Welcome, I'm server\n";
    Write(connfd, message, strlen(message));

    struct event *ev = event_new(base, connfd, EV_TIMEOUT | EV_READ | EV_PERSIST, read_cb, base);
    if(event_add(ev, NULL) == -1) {
        perror("event_add error");
        exit(-1);
    }

}

int main() {
    struct event_base *base;
    struct event *ev;
    int listenfd;
    struct sockaddr_in s_addr;
    socklen_t socklen = sizeof(s_addr);

    base = event_base_new();
    const char *method = event_base_get_method(base);   //查看设置的I/O模型
    printf("Method: %s\n", method);

    listenfd = Socket(AF_INET, SOCK_STREAM, 0);

    memset(&s_addr, 0, sizeof(s_addr));
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(8000);
    s_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    evutil_make_listen_socket_reuseable(listenfd);  //设置端口重用
    evutil_make_socket_nonblocking(listenfd);       //设置非阻塞

    Bind(listenfd, (struct sockaddr *)&s_addr, socklen);

    Listen(listenfd, 20);

    ev = evsignal_new(base, SIGINT, signal_cb, base);
    if(event_add(ev, NULL) == -1) {
        perror("event_add error");
        exit(-1);
    }

    ev = event_new(base, listenfd, EV_TIMEOUT | EV_READ | EV_WRITE | EV_PERSIST, accept_cb, base);
    if(event_add(ev, NULL) == -1) {
        perror("event_add error");
        exit(-1);
    }

    event_base_dispatch(base);

    return 0;
}
