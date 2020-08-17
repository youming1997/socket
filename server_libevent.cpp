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
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>

#include "include/wrap.h"
#include "include/websocket_common.h"
#include "include/sha1.h"
#include "include/base64.h"
#include <map>

#define LINE_MAX 1024
#define GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

void signal_cb(evutil_socket_t sockfd, short event, void *arg) {
    struct event_base *base = (struct event_base *)arg;
    struct timeval tv = {2, 0};
    printf("Caught a signal, will be cleaned in two seconds.\n");
    event_base_loopexit(base, &tv);
}


void read_cb(evutil_socket_t clntfd, short event, void *arg) {
    struct event_base *base = (struct event_base *)arg;
    char recvPackage[LINE_MAX];
    char recvBuf[LINE_MAX];
    unsigned int recvLen;

    memset(recvPackage, 0, LINE_MAX);
    memset(recvBuf, 0, LINE_MAX);
    int ret = recv(clntfd, recvPackage, LINE_MAX, MSG_NOSIGNAL);
    websocket_dePackage((unsigned char *)recvPackage, strlen(recvPackage), (unsigned char *)recvBuf, LINE_MAX, &recvLen);
    printf("Client said: %s", recvBuf);
    //sleep(1);
    //websocket_send(clntfd, (unsigned char *)recvBuf, ret, false, WCT_TXTDATA);
}
void write_cb() {}

void accept_cb(evutil_socket_t servfd, short event, void *arg) {
    struct event_base *base = (struct event_base *)arg;
    struct sockaddr_in c_addr;
    char recvBuf[LINE_MAX];
    socklen_t sockLen = sizeof(struct sockaddr);
    memset(&c_addr, 0, sizeof(c_addr));
    int connfd = Accept(servfd, (struct sockaddr *)&c_addr, (socklen_t *)&sockLen);

    memset(recvBuf, 0, LINE_MAX);
    int ret = recv(connfd, recvBuf, LINE_MAX, MSG_NOSIGNAL);
    if(-1 == ret) {

    }
    websocket_serverLinkToClient(connfd, recvBuf, LINE_MAX);
    if()
//    websocket_send(connfd, (unsigned char *)"Hello, I'm Server.\n", strlen("Hello, I'm Server.\n"), false, WCT_TXTDATA);

    struct event *ev = event_new(base, connfd, EV_TIMEOUT | EV_READ | EV_PERSIST, read_cb, base);
    if(event_add(ev, NULL) < 0) {
        perror("read_write_event error");
        event_free(ev);
    }

}

int main() {
    int listenfd;
    struct sockaddr_in s_addr;
    struct evconnlistener *listener;

    memset(&s_addr, 0, sizeof(struct sockaddr_in));
    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = INADDR_ANY;
    s_addr.sin_port = htons(8000);

    listenfd = Socket(AF_INET, SOCK_STREAM, 0);
    int old_option = setNonblocking(listenfd);

    while (Bind(listenfd, (struct sockaddr *)&s_addr, sizeof(struct sockaddr)) < 0)
        delayms(1);

    Listen(listenfd, 10);

    struct event_base *base = event_base_new();
    struct event *ev;


    ev = evsignal_new(base, SIGINT, signal_cb, base);
    printf("%s", ev);
    if(evsignal_add(ev, NULL) < 0) {
        perror_exit("signal_event error");
    }

    ev = event_new(base, listenfd, EV_TIMEOUT | EV_READ | EV_PERSIST, accept_cb, base);
    if(event_add(ev, NULL) < 0) {
        perror_exit("accept_event error");
    }

    event_base_dispatch(base);

    event_free(ev);
    event_base_free(base);

    return 0;
}
