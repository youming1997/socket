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

typedef enum {
    CS_CONNECTED,                   //已建立TCP连接
    CS_FAILED,                      //连接失败
    CS_WEBSOCKET_CONNECTED,         //已建立websocket连接
    CS_WEBSOCKET_FAILED,            //websocket连接失败
    CS_HANDLING,                    //数据包处理中
    CS_DISCONNECTED,                //断开连接
    CS_TIMEOUT                      //超时
}CLIENT_STATE;

struct client_rw{
    struct event *read_event;
    struct event *write_event;
    //char recvbuf[SAVE_MAX];
    //char sendbuf[SAVE_MAX];
    char buffer[SAVE_MAX];
    unsigned int buf_used;
    unsigned int recv_num;
    unsigned int recvPackage_num;
    unsigned int sendPackage_num;
    int timeout;
    CLIENT_STATE state;
};

struct client_rw *client_rw_init(struct event_base *base, int connfd) {
    struct client_rw *clientRw = (struct client_rw *)(malloc(sizeof(struct client_rw)));
    if(clientRw == NULL) {
        return NULL;
    }
    //memset(clientRw->recvbuf, '\0', SAVE_MAX);
    //memset(clientRw->sendbuf, '\0', SAVE_MAX);
    memset(clientRw->buffer, 0, SAVE_MAX);
    clientRw->recv_num = 0;
    clientRw->recvPackage_num = 0;
    clientRw->sendPackage_num = 0;
    clientRw->buf_used = 0;
    clientRw->timeout = 0;
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
    //printf("read_cb OK\n");
    struct client_rw *clientRw = (struct client_rw *)arg;
    //char recvPackage[LINE_MAX];

    //memset(recvPackage, 0, SAVE_MAX);
    int /*i, j,*/ ret;
    ret = recv(clntfd, clientRw->buffer, SAVE_MAX, 0);
    if(ret == -1) {
        perror("recv error");
        client_rw_free(clientRw);
//        break;
    }
    if(ret == 0) {
        return ;
    }
    printf("recv OK, ret = %d\n", ret);
    clientRw->buf_used += ret;
//    clientRw->recv_num = ret;
    if(event_add(clientRw->write_event, NULL) < 0) {
        perror("write_event error");
        client_rw_free(clientRw);
        close(clntfd);
    }
}

//void state_check(client_rw clientRw) {

//}

void write_cb(evutil_socket_t sockfd, short event, void *arg) {
//    printf("write_cb 调用\n");
    struct client_rw *clientRw = (struct client_rw *)arg;
    WebSocket_CommunicationType type;
    unsigned char recvPackage[SAVE_MAX],/* *sendPackage,*/ message[LINE_MAX];
    unsigned char maskKey[4];
    unsigned int headStart, headEnd, i;
    int dataStart, dataLen, payLoadLen;
    int isMask;

    //printf("write_cb 调用\n");
    if(clientRw->buf_used != 0) {
//        printf("write_cb 调用\n");
        for(i = 0; i < clientRw->buf_used && clientRw->state == CS_CONNECTED; ++i) {
            if(clientRw->buffer[i] == 'G' && clientRw->buffer[i + 1] == 'E' && clientRw->buffer[i + 2] == 'T') {
                headStart = i;
                printf("GET 获取\n");
                continue;
            }
            if(clientRw->buffer[i] == '\r' && clientRw->buffer[i + 1] == '\n' && clientRw->buffer[i + 2] == '\r' && clientRw->buffer[i + 3] == '\n') {
                headEnd = i + 3;
                printf("尾获取\n");
                websocket_serverLinkToClient(sockfd, (clientRw->buffer + headStart), (headEnd - headStart + 1));
                printf("连接成功\n");
                /*headEnd++;
                for(; clientRw->buffer + headEnd != NULL; ++headStart, ++headEnd) {
                    clientRw->buffer[headStart] = clientRw->buffer[headEnd];
                    clientRw->buffer[headEnd] = '\0';
                }
                //memset(clientRw->buffer + headStart, 0, headEnd - headStart);
                clientRw->state = CS_WEBSOCKET_CONNECTED;
                return;
                */
                memset(clientRw->buffer, 0, SAVE_MAX);
                clientRw->buf_used = 0;
                clientRw->state = CS_WEBSOCKET_CONNECTED;
                return ;
            }
        }

        if(clientRw->state == CS_WEBSOCKET_CONNECTED) {
            printf("websocket 接收消息中\n");
            type = websocket_isType((unsigned char *)clientRw->buffer, 1);
            if(type == WCT_BINDATA || type == WCT_TXTDATA || type == WCT_PING) {
                clientRw->state = CS_HANDLING;
                isMask = websocket_isMask((unsigned char *)clientRw->buffer, 2);
                if(isMask) {
                    clientRw->recvPackage_num += 4;
                }
                printf("isMask 后 clientRw->recvPackage_num = %d\n", clientRw->recvPackage_num);
                dataLen = websocket_isDataLen((unsigned char *)clientRw->buffer, clientRw->buf_used, isMask, maskKey, &dataStart, &payLoadLen);
                clientRw->recvPackage_num += dataLen;
                printf("加 dataLen 后 clientRw->recvPackage_num = %d\n", clientRw->recvPackage_num);
                if(payLoadLen == 126) {
                    clientRw->recvPackage_num += 4;
                } else if(payLoadLen == 127) {
                    clientRw->recvPackage_num += 10;
                } else {
                    clientRw->recvPackage_num += 2;
                }
                printf("加数据长度的数字后 clientRw->recvPackage_num = %d\n", clientRw->recvPackage_num);
            } else if(type == WCT_DISCONN) {
                printf("client has been shutdown.\n");
                client_rw_free(clientRw);
                close(sockfd);
            }
            clientRw->timeout = 0;
            printf("接收消息成功\n");
        }

        if(clientRw->state == CS_HANDLING && clientRw->recvPackage_num <= clientRw->buf_used) {
            printf("处理消息中\n");
            printf("clientRw->recvPackage_num = %d\n", clientRw->recvPackage_num);
//            recvPackage = (unsigned char *)malloc(sizeof(char) * (clientRw->recvPackage_num + 1));
//            message = (unsigned char *)malloc(sizeof(char) * clientRw->recvPackage_num);
            memset(recvPackage, 0, SAVE_MAX);
            memcpy(recvPackage, clientRw->buffer, clientRw->recvPackage_num);
            memset(message, 0, LINE_MAX);
            
            printf("recvPackage 和 message 建立成功\n");
            
            websocket_dePackage(recvPackage, clientRw->recvPackage_num, message, SAVE_MAX, &clientRw->recv_num);
//            message[clientRw->recv_num] = '\0';
            printf("解包成功\n");
            printf("Client said: %s\n", message);
            
//            free(message);
//            free(recvPackage);
            clientRw->buf_used -= clientRw->recvPackage_num;
            clientRw->recvPackage_num = 0;
            clientRw->state = CS_WEBSOCKET_CONNECTED;
            printf("处理消息成功\n");
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
    printf("accept OK\n");

    evutil_make_socket_nonblocking(connfd);

    struct client_rw *clientRw = client_rw_init(base, connfd);
    if(clientRw == NULL) {
        printf("create client failed.\n");
        close(connfd);
    } else {
        if(event_add(clientRw->read_event, NULL) < 0) {
            perror("read_event error");
            client_rw_free(clientRw);
            close(connfd);
            return ;
        }
        clientRw->state = CS_CONNECTED;
        printf("struct client_rw OK\n");
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
