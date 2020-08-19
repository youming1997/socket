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

struct client_rw{
    struct event *read_event;
    struct event *write_event;
    char recvbuf[SAVE_MAX];
    char sendbuf[SAVE_MAX];
    size_t buf_used;
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
    clientRw->buf_used = 0;
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

    return clientRw;
}

void client_rw_free(struct client_rw *clientRw) {
    if(clientRw->read_event != NULL)
        event_free(clientRw->read_event);
    if(clientRw->write_event != NULL)
        event_free(clientRw->write_event);
    free(clientRw);
}

void read_cb(evutil_socket_t clntfd, short event, void *arg) {
    struct client_rw *clientRw = (struct client_rw *)arg;
    char recvPackage[SAVE_MAX];

    while(1) {
        memset(recvPackage, 0, SAVE_MAX);
        int ret = recv(clntfd, recvPackage, SAVE_MAX, MSG_NOSIGNAL);
        if(ret == -1) {
            perror("recv error");
            client_rw_free(clientRw);
            break;
        } else if(ret == 0) {
            printf("client has been shutdown.\n");
            client_rw_free(clientRw);
            close(clntfd);
            break;
        }
        printf("recv OK, ret = %d\n", ret);
        if(strstr(recvPackage, "GET") != NULL) {
            websocket_serverLinkToClient(clntfd, recvPackage, ret);
        } else {
            ret = websocket_dePackage((unsigned char *)recvPackage, strlen(recvPackage), (unsigned char *)clientRw->recvbuf, SAVE_MAX, (unsigned int *)&clientRw->recv_num);
            printf("ret = %d\n", ret);
            printf("Client said: %s\n", clientRw->recvbuf);
            char sendPackage[SAVE_MAX];
            memset(sendPackage, 0, SAVE_MAX);
            int len = websocket_enPackage((unsigned char *)clientRw->recvbuf, clientRw->recv_num, (unsigned char *)sendPackage, SAVE_MAX, false, (WebSocket_CommunicationType)ret);
            int ret = send(clntfd, sendPackage, len, MSG_NOSIGNAL);
            if(-1 == ret) {
                perror("send error");
                client_rw_free(clientRw);
            }
        }    
    }
}

void write_cb(evutil_socket_t sockfd, short event, void *arg) {
    //printf("write_cb 调用\n");
    struct client_rw *clientRw = (struct client_rw *)arg;
    unsigned char sendPackage[SAVE_MAX];
    
    printf("write_cb 调用\n");
    while(strlen(clientRw->recvbuf) != 0) {
        strncpy(clientRw->sendbuf, clientRw->recvbuf, clientRw->recv_num);
        clientRw->send_num = clientRw->recv_num;
        int len = websocket_enPackage((unsigned char *)clientRw->sendbuf, clientRw->send_num, sendPackage, SAVE_MAX, false, WCT_TXTDATA);
        int ret = send(sockfd, sendPackage, len, MSG_NOSIGNAL);
        if(-1 == ret) {
            perror("send error");
            client_rw_free(clientRw);
        }
    }
}

void accept_cb(evutil_socket_t servfd, short event, void *arg) {
    struct event_base *base = (struct event_base *)arg;
    struct sockaddr_in c_addr;
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
    printf("socket OK\n");

//    int old_option = setNonblocking(listenfd);
//    int on = 1;
//    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    evutil_make_socket_nonblocking(listenfd);
    evutil_make_listen_socket_reuseable(listenfd);

    int ret = bind(listenfd, (struct sockaddr *)&s_addr, sizeof(struct sockaddr));
    if(-1 == ret) {
        perror("bind error");
        close(listenfd);
        exit(1);
    }
    printf("bind OK\n");
    
    ret = listen(listenfd, 10);
    if(-1 == ret) {
        perror("listen error");
        close(listenfd);
        exit(1);
    }
    printf("listen OK\n");

    struct event_base *base = event_base_new();
    struct event *ev;

    ev = event_new(base, listenfd, EV_TIMEOUT | EV_READ | EV_PERSIST, accept_cb, base);
    if(event_add(ev, NULL) < 0) {
        perror("accept_event error");
    }

    event_base_dispatch(base);

    event_free(ev);
    event_base_free(base);

    return 0;
}
