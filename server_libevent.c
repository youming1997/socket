#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <limits.h>

#include <event2/event.h>

#include "include/websocket_common.h"

#define PORT 8000
#define CLIENT_MAXNUM 100
//#define SAVE_MAX (16 * 1024)

//typedef enum client_state {
//    CS_TCP_CONNECTED,                   //TCP连接成功
//    //CS_TCP_FAILED,                      //TCP连接失败
//    CS_WEBSOCKET_CONNECTED,             //websocket连接成功
//    //CS_WEBSOCKET_FAILED,                //websocket连接失败
//    CS_SENDING,                         //发送消息中
//    CS_HANDLING,                        //数据包处理中
//    CS_OVERTIME,                        //超时
//    CS_CLOSED,                          //已关闭
//};
//
//typedef struct client_rw {
//    int clntfd;                         //客户端句柄
//    struct sockaddr_in clnt_addr;       //客户端相关信息
//    struct event *read_event;           //从客户端读数据的方法
//    struct event *write_event;          //向客户端写数据的方法
//    unsigned char recv_buf[SAVE_MAX];   //从客户端读的数据
//    unsigned char send_buf[SAVE_MAX];   //向客户端写的数据
//    unsigned long recv_num;             //读到的数据的长度
//    unsigned long send_num;             //写出的数据的长度
//    pthread_mutex_t recv_lock;          //读锁
//    pthread_mutex_t send_lock;          //写锁
//    pthread_mutex_t handle_lock;        //数据处理锁
//    enum client_state state;            //客户端状态
//};

struct client_rw g_clients[CLIENT_MAXNUM];
int g_client_num = 0;
int g_flag = 1;
pthread_mutex_t handle_lock;        //数据处理锁

struct client_rw *client_rw_init(int clntfd, struct sockaddr_in c_addr, struct event_base *base);
void client_rw_free(struct client_rw *clientRw);
void accept_cb(int servfd, short event, void *arg);
void read_cb(int clntfd, short event, void *arg);
void write_cb(int clntfd, short event, void *arg);
static void *check_state(void *base);

int main() {
    int servfd;
    struct sockaddr_in s_addr;
    socklen_t sockLen = sizeof(struct sockaddr_in);
    int ret;

    memset(&s_addr, 0, sockLen);
    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    s_addr.sin_port = htons(PORT);

    servfd = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == servfd) {
        printf("socket error: %s, errno: %d", strerror(errno), errno);
        exit(1);
    }
    printf("socket OK\n");

    ret = bind(servfd, (struct sockaddr *)&s_addr, sockLen);
    if(-1 == ret) {
        printf("bind error: %s, errno: %d", strerror(errno), errno);
        close(servfd);
        exit(1);
    }
    printf("bind OK\n");

//    int on = 1;
//    setsockopt(servfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
//    int old_option = fcntl(servfd, F_GETFL, 0);
//    fcntl(servfd, F_SETFL, old_option | O_NONBLOCK);
    evutil_make_socket_nonblocking(servfd);
    evutil_make_listen_socket_reuseable(servfd);

    ret = listen(servfd, CLIENT_MAXNUM);
    if(-1 == ret) {
        printf("listen error: %s, errno: %d", strerror(errno), errno);
        close(servfd);
        exit(1);
    }
    printf("listen OK\n");

    struct event_base *base = event_base_new();
    struct event *accept_event;

    accept_event = event_new(base, servfd, EV_TIMEOUT | EV_READ | EV_PERSIST, accept_cb, base);

    if(event_add(accept_event, NULL) < 0) {
        printf("accept_event error: %s, errno: %d", strerror(errno), errno);
        close(servfd);
        event_free(accept_event);
        event_base_free(base);
        exit(1);
    }

    pthread_t workpid;
    pthread_create(&workpid, NULL, check_state, (void *)base);
    event_base_dispatch(base);

    g_flag = 0;
    pthread_join(workpid, NULL);
    event_base_free(base);

    return 0;
}

struct client_rw *client_rw_init(int clntfd, struct sockaddr_in c_addr, struct event_base *base) {
    struct client_rw *clientRw = (struct client_rw *)malloc(sizeof(struct client_rw));
    if(clientRw == NULL) {
        return NULL;
    }
    clientRw->clntfd = clntfd;
    clientRw->clnt_addr = c_addr;
    memset(clientRw->recv_buf, 0, SAVE_MAX);
    memset(clientRw->send_buf, 0, SAVE_MAX);
    clientRw->recv_num = 0;
    clientRw->send_num = 0;
    pthread_mutex_init(&clientRw->recv_lock, NULL);
    pthread_mutex_init(&clientRw->send_lock, NULL);
    clientRw->state = CS_TCP_CONNECTED;
    clientRw->read_event = event_new(base, clntfd, EV_READ | EV_PERSIST, read_cb, clientRw);
    if(clientRw->read_event == NULL) {
        printf("read_event error: %s, errno: %d", strerror(errno), errno);
        free(clientRw);
        return NULL;
    }
    clientRw->write_event = event_new(base, clntfd, EV_WRITE | EV_PERSIST, write_cb, clientRw);
    if(clientRw->write_event == NULL) {
        printf("write_event error: %s, errno: %d", strerror(errno), errno);
        event_free(clientRw->read_event);
        free(clientRw);
        return NULL;
    }

    return clientRw;
}

void client_rw_free(struct client_rw *clientRw) {
    if(clientRw != NULL) {
        pthread_mutex_destroy(&clientRw->recv_lock);
        pthread_mutex_destroy(&clientRw->send_lock);
        if(clientRw->read_event != NULL)
            event_free(clientRw->read_event);
        if(clientRw->write_event != NULL)
            event_free(clientRw->write_event);
    }
}

void client_rw_del(int clntfd) {
    int i, j;
    pthread_mutex_lock(&handle_lock);
    for (i = 0; i < g_client_num; ++i) {
        if (g_clients[i].clntfd == clntfd) {
            client_rw_free(&g_clients[i]);
            close(g_clients[i].clntfd);
            for (j = i; j + 1 < g_client_num; ++j) {
                g_clients[j] = g_clients[j + 1];
            }
            g_client_num--;
            return;
        }
    }
    pthread_mutex_unlock(&handle_lock);
}

static void *check_state(void *base) {
    int i, j;
    while(g_flag) {
        pthread_mutex_lock(&handle_lock);
        for(i = 0; i < g_client_num; ++i) {

            if(g_clients[i].state == CS_TCP_CONNECTED) {
                int ret = websocket_getHead(&g_clients[i]);
                if(ret == 1) {
                    g_clients[i].state = CS_WEBSOCKET_CONNECTED;
                }
                continue;
            }
            if(g_clients[i].state == CS_WEBSOCKET_CONNECTED) {
                if(g_clients[i].recv_num != 0) {
                    unsigned char *message_ptr;
                    unsigned int recvPackageLen;
                    WebSocket_CommunicationType type = websocket_getType(g_clients[i].recv_buf, g_clients[i].recv_num);
                    message_ptr = (unsigned char *)malloc(sizeof(unsigned char) * LINE_MAX);
                    
                    int ret = websocket_getRecvPackage(&g_clients[i], message_ptr, &recvPackageLen);
                    if(ret == WCT_PONG) {
                        for(i = 0; message_ptr[i + strlen(DP_PONG) + 1] != 0; ++i) {
                            message_ptr[i] = message_ptr[i + strlen(DP_PONG) + 1];
                        }
                        memset(message_ptr + i, 0, strlen((char *)message_ptr) - i);
                        recvPackageLen -= strlen(DP_PONG);
                        pthread_mutex_lock(&g_clients[i].send_lock);
                        memcpy(g_clients[i].send_buf, message_ptr, recvPackageLen);
                        g_clients[i].send_num += recvPackageLen;
                        pthread_mutex_unlock(&g_clients[i].send_lock);

                        free(message_ptr);
                    } else if(ret == -1) {
                        free(message_ptr);
                        continue;
                    } else if(ret == 0) {
                        free(message_ptr);
                        continue;
                    } else if(ret == 1){
                        char sendPackage[LINE_MAX];
                        sprintf(sendPackage, "Client(clntfd = %d) said: %s", g_clients[i].clntfd, message_ptr);
                        printf("Client(clntfd = %d) said: %s\n", g_clients[i].clntfd, message_ptr);
                        printf("strlen(sendPackage) = %lu\n", strlen(sendPackage));
                        int ret = websocket_enPackage((unsigned char *)sendPackage, strlen(sendPackage), message_ptr, LINE_MAX, false, type);
                        if(ret == -1) {
                            printf("打包出现问题\n");
                            free(message_ptr);
                            continue;
                        }
                        for(j = 0; j < g_client_num; ++j) {
                            if(g_clients[j].state == CS_WEBSOCKET_CONNECTED) {
                                if(g_clients[j].clntfd != g_clients[i].clntfd) {

                                    pthread_mutex_lock(&g_clients[j].send_lock);
                                    memcpy(g_clients[j].send_buf, message_ptr, ret);
                                    g_clients[j].send_num = ret;
                                    pthread_mutex_unlock(&g_clients[j].send_lock);
                                    
                                }
                            }
                        }
                        if(message_ptr != NULL)
                            free(message_ptr);
                    }
                }
            }
            if(g_clients[i].state == CS_CLOSED) {
                printf("client(clntfd = %d) has been shutdown\n", g_clients[i].clntfd);
                close(g_clients[i].clntfd);
                client_rw_free(&g_clients[i]);
                continue;
            }
        }
        pthread_mutex_unlock(&handle_lock);
        sleep(1);
    }
//        if(g_client_num == 0) {
//            overtime++;
//            if(overtime >= 6000) {
//                printf("一分钟内没有客户端连接，服务端将在5秒后关闭\n");
//                struct timeval tv = {5, 0};
//                event_base_loopexit(base, &tv);
//                g_flag = 0;
//            }
//        }
    return 0;
}

void accept_cb(int servfd, short event, void *arg) {
    struct event_base *base = (struct event_base *)arg;
    int clntfd;
    struct sockaddr_in c_addr;
    socklen_t sockLen = sizeof(struct sockaddr_in);
    struct client_rw *clientRw;

    memset(&c_addr, 0, sockLen);
    clntfd = accept(servfd, (struct sockaddr *)&c_addr, &sockLen);
    if(-1 == clntfd) {
        printf("accept error: %s, errno: %d", strerror(errno), errno);
        return ;
    }
    printf("accept OK\n");

    clientRw = client_rw_init(clntfd, c_addr, base);
    g_clients[g_client_num] = *clientRw;
    if(event_add(g_clients[g_client_num].read_event, NULL) < 0) {
        printf("read_event add error: %s, errno: %d", strerror(errno), errno);
        client_rw_free(clientRw);
        client_rw_free(&g_clients[g_client_num]);
        close(clntfd);
        return ;
    }
    if(event_add(g_clients[g_client_num].write_event, NULL) < 0) {
        printf("write_event add error: %s, errno: %d", strerror(errno), errno);
        client_rw_free(clientRw);
        client_rw_free(&g_clients[g_client_num]);
        close(clntfd);
        return ;
    }
    g_client_num++;
}

void read_cb(int clntfd, short event, void *arg) {
    int ret, i, j;
    char recvPackage[LINE_MAX];
    for(i = 0; i < g_client_num; ++i) {
        if (g_clients[i].clntfd == clntfd) {
            ret = recv(clntfd, recvPackage, LINE_MAX, 0);
            if(ret == -1) {
                printf("recv error: %s, errno: %d", strerror(errno), errno);
                if(errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN) {
                    return ;
                } else {
                    client_rw_del(clntfd);
                    return ;
                }
            } else if(ret == 0) {
                client_rw_del(clntfd);
                return ;
            }

        pthread_mutex_lock(&g_clients[i].recv_lock);
        memcpy(g_clients[i].recv_buf + g_clients[i].recv_num, recvPackage, ret);
        g_clients[i].recv_num += ret;
        printf("g_clients[%d].recv_buf = ", i);
        for(j = 0; j < g_clients[i].recv_num; ++j) {
            printf("%.2x ", g_clients[i].recv_buf[j]);
        }
        printf("\n");
        pthread_mutex_unlock(&g_clients[i].recv_lock);

        break;
        }
    }
}

void write_cb(int clntfd, short event, void *arg) {
    int ret, i, j;
    char sendPackage[LINE_MAX];
    for(i = 0; i < g_client_num; ++i) {
        if(g_clients[i].clntfd == clntfd) {
            break;
        }
    }
    if(i == g_client_num && g_clients[i].clntfd != clntfd) {
        return ;
    }
    if(g_clients[i].send_num != 0) {

        pthread_mutex_lock(&g_clients[i].send_lock);
        printf("send_buf = ");
        for(j = 0; j < g_clients[i].send_num; ++j) {
            printf("%.2x ", g_clients[i].send_buf[j]);
        }
        printf("\n");
        printf("send_num = %lu\n", g_clients[i].send_num);
        int sendnum = g_clients[i].send_num;
        memcpy(sendPackage, g_clients[i].send_buf, g_clients[i].send_num);
//        for(j = 0; g_clients[i].send_buf[j + g_clients[i].send_num] != 0; ++j) {
//            g_clients[i].send_buf[j] = g_clients[i].send_buf[j + g_clients[i].send_num];
//        }
//        memset(g_clients[i].send_buf + j, 0, SAVE_MAX - j);
        pthread_mutex_unlock(&g_clients[i].send_lock);

        ret = send(clntfd, sendPackage, sendnum, 0);
        if(ret == -1) {
            if (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN) {
                return;
            } else {
                printf("send error: %s, errno: %d", strerror(errno), errno);
                client_rw_del(clntfd);
            }
        } else if(ret == 0) {
            return ;
        } else if(ret < sendnum) {
            pthread_mutex_lock(&g_clients[i].send_lock);
            memcpy(g_clients[i].send_buf, sendPackage + ret, sendnum - ret);
            memset(g_clients[i].send_buf + (sendnum - ret), 0, SAVE_MAX - (sendnum - ret));
            g_clients[i].send_num -= (sendnum - ret);
            pthread_mutex_unlock(&g_clients[i].send_lock);
        } else if(ret == sendnum) {
            pthread_mutex_lock(&g_clients[i].send_lock);
            memset(g_clients[i].send_buf, 0, SAVE_MAX);
            g_clients[i].send_num = 0;
            pthread_mutex_unlock(&g_clients[i].send_lock);
        }
    }
}
