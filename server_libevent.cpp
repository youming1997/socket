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
#include <signal.h>

#include <event2/event.h>
#include <event2/bufferevent.h>

#include "include/websocket_common.h"

#define SAVE_MAX 16384
#define LINE_MAX 1024

void signal_cb(evutil_socket_t sockfd, short event, void *arg);
void read_cb(evutil_socket_t clntfd, short event, void *arg);
void write_cb(evutil_socket_t sockfd, short event, void *arg);
void accept_cb(evutil_socket_t servfd, short event, void *arg);

typedef struct client_rw{
    struct event *read_event;
    struct event *write_event;
    char recvbuf[SAVE_MAX];
    char sendbuf[SAVE_MAX];
    size_t recv_num;
    size_t send_num;
};

struct client_rw *client_rw_init(struct event_base *base, int connfd) {
    struct client_rw *clientRw = (struct client_rw *)(malloc(sizeof(struct client_rw)));
    if(clientRw == NULL) {
        return NULL;
    }
    memset(clientRw->recvbuf, '\0', SAVE_MAX);
    memset(clientRw->sendbuf, '\0', SAVE_MAX);
    clientRw->recv_num = 0;
    clientRw->send_num = 0;
    clientRw->read_event = event_new(base, connfd, EV_READ | EV_PERSIST, read_cb, clientRw);
    if(clientRw->read_event == NULL) {
        perror("create read_event failed");
        free(clientRw);
        return NULL;
    }
    clientRw->write_event = event_new(base, connfd, EV_WRITE | EV_PERSIST, write_cb, clientRw);
    if(clientRw->write_event == NULL) {
        perror("create write_event failed");
        event_free(clientRw->read_event);
        free(clientRw);
        return NULL;
    }
}

void client_rw_free(struct client_rw *clientRw) {
    if(clientRw->read_event != NULL)
        event_free(clientRw->read_event);
    if(clientRw->write_event != NULL)
        event_free(clientRw->write_event);
    free(clientRw);
}

void signal_cb(evutil_socket_t sockfd, short event, void *arg) {
    struct event_base *base = (struct event_base *)arg;
    struct timeval tv = {2, 0};
    printf("Caught a signal, will be cleaned in two seconds.\n");
    event_base_loopexit(base, &tv);
}

void read_cb(evutil_socket_t clntfd, short event, void *arg) {
    struct client_rw *clientRw = (struct client_rw *)arg;
    char recvPackage[SAVE_MAX];
    unsigned int recvLen;

    memset(recvPackage, 0, SAVE_MAX);
    int ret = recv(clntfd, recvPackage, SAVE_MAX, 0);
    websocket_dePackage((unsigned char *)recvPackage, strlen(recvPackage), (unsigned char *)clientRw->recvbuf, SAVE_MAX, (unsigned int *)&clientRw->recv_num);
    printf("Client said: %s", clientRw->recvbuf);
}
void write_cb(evutil_socket_t sockfd, short event, void *arg) {
    struct client_rw *clientRw = (struct client_rw *)arg;
    unsigned char sendPackage[SAVE_MAX];

    while(strlen(clientRw->recvbuf) != 0) {
        strncpy(clientRw->sendbuf, clientRw->recvbuf, clientRw->recv_num);
        clientRw->send_num = clientRw->recv_num;
        int len = websocket_enPackage((unsigned char *)clientRw->sendbuf, clientRw->send_num, sendPackage, SAVE_MAX, false, WCT_TXTDATA);
        int ret = send(sockfd, sendPackage, len, 0);
    }
}

void accept_cb(evutil_socket_t servfd, short event, void *arg) {
    struct event_base *base = (struct event_base *)arg;
    struct sockaddr_in c_addr;
    char recvBuf[LINE_MAX];
    socklen_t sockLen = sizeof(struct sockaddr);
    memset(&c_addr, 0, sizeof(c_addr));
    int connfd = accept(servfd, (struct sockaddr *)&c_addr, (socklen_t *)&sockLen);
    if(-1 == connfd) {
        perror("accept error");
    }

    struct client_rw *clientRw = client_rw_init(base, connfd);
    if(clientRw == NULL) {
        printf("create client failed.\n");
    } else {

        if(event_add(clientRw->read_event, NULL) < 0) {
            perror("read_event error");
        }
        if(event_add(clientRw->write_event, NULL) < 0) {
            perror("write_event error");
        }
    }
}

int main() {
    int listenfd;
    struct sockaddr_in s_addr;

    memset(&s_addr, 0, sizeof(struct sockaddr_in));
    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = INADDR_ANY;
    s_addr.sin_port = htons(8000);

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == listenfd) {
        perror("socket error");
        exit(1);
    }
    int old_option = setNonblocking(listenfd);
    int ret = bind(listenfd, (struct sockaddr *)&s_addr, sizeof(struct sockaddr));
    if(-1 == ret) {
        perror("bind error");
        close(listenfd);
        exit(1);
    }

    ret = listen(listenfd, 10);
    if(-1 == ret) {
        perror("listen error");
        close(listenfd);
        exit(1);
    }

    struct event_base *base = event_base_new();
    struct event *ev;


    ev = evsignal_new(base, SIGINT, signal_cb, base);
    printf("%s", ev);
    if(evsignal_add(ev, NULL) < 0) {
        perror("signal_event error");
    }
    event_priority_set(ev, 1);

    ev = event_new(base, listenfd, EV_TIMEOUT | EV_READ | EV_PERSIST, accept_cb, base);
    if(event_add(ev, NULL) < 0) {
        perror("accept_event error");
    }
    event_priority_set(ev, 0);

    event_base_dispatch(base);

    event_free(ev);
    event_base_free(base);

    return 0;
}


//#define LINE_MAX 1024
//
////event中的函数模型
////void (*event_callback_fn)(evutil_socket_t, short, void *)
//void signal_cb(evutil_socket_t sockfd, short event, void *arg) {
//    struct timeval tv = {2, 0};
//    struct event_base *base = (struct event_base *)arg;
//
//    printf("Caught a signal, will be clean in 2 seconds...\n");
//    event_base_loopexit(base, &tv);
//}
//
//void read_cb(evutil_socket_t clientfd, short event, void * arg) {
//    char buf[LINE_MAX];
//    int recvlen = Read(clientfd, buf, LINE_MAX);
//    if(-1 == recvlen) {
//        perror_exit("read error");
//    } else if(recvlen == 0) {
//        printf("Connection as been shutdown.\n");
//        close(clientfd);
//        return ;
//    } else {
//        buf[recvlen] = '\0';
//        printf("client %d said: %s", clientfd, buf);
//        int sendlen = Write(clientfd, buf, recvlen);
//        if(-1 == sendlen) {
//            perror_exit("send error");
//        }
//    }
//}
//
//void accept_cb(evutil_socket_t serverfd, short event, void *arg) {
//    int connfd;
//    struct sockaddr_in c_addr;
//    socklen_t socklen = sizeof(struct sockaddr_in);
//
//    connfd = Accept(serverfd, (struct sockaddr *)&c_addr, &socklen);
//
//    struct event_base *base = (struct event_base *)arg;
//
//    char message[] = "Welcome, I'm server\n";
//    Write(connfd, message, strlen(message));
//
//    struct event *ev = event_new(base, connfd, EV_TIMEOUT | EV_READ | EV_PERSIST, read_cb, base);
//    if(event_add(ev, NULL) == -1) {
//        perror("event_add error");
//        exit(-1);
//    }
//
//}
//
//int main() {
//    struct event_base *base;
//    struct event *ev;
//    int listenfd;
//    struct sockaddr_in s_addr;
//    socklen_t socklen = sizeof(s_addr);
//
//    base = event_base_new();
//    const char *method = event_base_get_method(base);   //查看设置的I/O模型
//    printf("Method: %s\n", method);
//
//    listenfd = Socket(AF_INET, SOCK_STREAM, 0);
//
//    memset(&s_addr, 0, sizeof(s_addr));
//    s_addr.sin_family = AF_INET;
//    s_addr.sin_port = htons(8000);
//    s_addr.sin_addr.s_addr = htonl(INADDR_ANY);
//
//    evutil_make_listen_socket_reuseable(listenfd);  //设置端口重用
//    evutil_make_socket_nonblocking(listenfd);       //设置非阻塞
//
//    Bind(listenfd, (struct sockaddr *)&s_addr, socklen);
//
//    Listen(listenfd, 20);
//
//    ev = evsignal_new(base, SIGINT, signal_cb, base);
//    if(event_add(ev, NULL) == -1) {
//        perror("event_add error");
//        exit(-1);
//    }
//
//    ev = event_new(base, listenfd, EV_TIMEOUT | EV_READ | EV_WRITE | EV_PERSIST, accept_cb, base);
//    if(event_add(ev, NULL) == -1) {
//        perror("event_add error");
//        exit(-1);
//    }
//
//    event_base_dispatch(base);
//
//    return 0;
//}
