#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <limits.h>
//#include <list>
#include <map>

#include <event2/event.h>

#include "include/websocket_common.h"


#define PORT 8000
#define CLIENT_MAXNUM 100
//#define SAVE_MAX (16 * 1024)

//enum client_state {
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
//struct client_rw {
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
////    pthread_mutex_t handle_lock;        //数据处理锁
//    enum client_state state;            //客户端状态
//};

std::map<int, struct client_rw> g_clients;
//std::list<struct client_rw> g_clients;
//struct client_rw g_clients[CLIENT_MAXNUM];
int g_flag = 1;
pthread_mutex_t handle_lock;        //数据处理锁

struct client_rw *client_rw_init(int clntfd, struct sockaddr_in c_addr, struct event_base *base);
void client_rw_free(std::map<int, struct client_rw>::iterator it);
void accept_cb(int servfd, short event, void *arg);
void read_cb(int clntfd, short event, void *arg);
void write_cb(int clntfd, short event, void *arg);
void *check_state(void *base);

int main() {
    int servfd;
    struct sockaddr_in s_addr;
    socklen_t sockLen = sizeof(struct sockaddr_in);
    int ret;

    pthread_mutex_init(&handle_lock, NULL);
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
    
    evutil_make_listen_socket_reuseable(servfd);

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

    ret = listen(servfd, CLIENT_MAXNUM);
    if(-1 == ret) {
        printf("listen error: %s, errno: %d", strerror(errno), errno);
        close(servfd);
        exit(1);
    }
    printf("listen OK\n");

    struct event_base *base = event_base_new();
    struct event *accept_event;

    accept_event = event_new(base, servfd, EV_READ | EV_PERSIST, accept_cb, base);

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
    pthread_kill(workpid, SIGQUIT);
    event_base_free(base);

    return 0;
}

struct client_rw *client_rw_init(int clntfd, struct sockaddr_in c_addr, struct event_base *base) {
    struct client_rw *clientRw = (struct client_rw *)malloc(sizeof(struct client_rw));
    if(clientRw == NULL) {
        return NULL;
    }
    //clientRw->clntfd = clntfd;
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

void client_rw_free(std::map<int, struct client_rw>::iterator it) {
    printf("free client_rw start\n");
    if(it->first != -1) {
        pthread_mutex_destroy(&it->second.recv_lock);
        pthread_mutex_destroy(&it->second.send_lock);
        printf("destroy lock success\n");
//        memset(it->second.recv_buf, 0, SAVE_MAX);
//        memset(it->second.send_buf, 0, SAVE_MAX);
//        printf("set array zero success\n");
        if(it->second.read_event != NULL) {
            event_free(it->second.read_event);
            printf("read_event free\n");
//            clientRw->read_event = NULL;
        }
        if(it->second.write_event != NULL) {
            event_free(it->second.write_event);
            printf("write_event free\n");
//            clientRw->write_event = NULL;
        }
        close(it->first);
        g_clients.erase(it);
//        free(clientRw);
//        clientRw = NULL;
    }
    printf("free client_rw OK\n");
}

void client_rw_del(int clntfd) {
    printf("delete client_rw start\n");
    auto it_client = g_clients.find(clntfd);
    if (it_client != g_clients.end()) {
        client_rw_free(it_client);
        printf("delete client_rw OK\n");
        return;
    } else {
        printf("no such client which clntfd = %d", clntfd);
        return ;
    }
}

void *check_state(void *base) {
    int i;
    std::map<int, struct client_rw>::iterator it_clients = g_clients.begin();

    while(g_flag) {
//        printf("lock\n");
        pthread_mutex_lock(&handle_lock);
        it_clients = g_clients.begin();
        for(; it_clients != g_clients.end(); ++it_clients) {

            if(it_clients->second.state == CS_TCP_CONNECTED) {
                int ret = websocket_getHead(it_clients);
                if(ret == 1) {
                    it_clients->second.state = CS_WEBSOCKET_CONNECTED;
                } else if(ret == 0) {
                    printf("clinet error\n");
                    client_rw_del(it_clients->first);
                }
                continue;
            }
            if(it_clients->second.state == CS_WEBSOCKET_CONNECTED) {
                if(it_clients->second.recv_num != 0) {
                    unsigned char *message_ptr;
                    unsigned int recvPackageLen;
                    WebSocket_CommunicationType type = websocket_getType(it_clients->second.recv_buf, it_clients->second.recv_num);

                    message_ptr = (unsigned char *)malloc(sizeof(unsigned char) * LINE_MAX);
                    int ret = websocket_getRecvPackage(&(it_clients->second), message_ptr, &recvPackageLen);
                    if(ret == WCT_PONG) {
                        for(i = 0; message_ptr[i + strlen(DP_PONG) + 1] != 0; ++i) {
                            message_ptr[i] = message_ptr[i + strlen(DP_PONG) + 1];
                        }
                        memset(message_ptr + i, 0, strlen((char *)message_ptr) - i);
                        recvPackageLen -= strlen(DP_PONG);
                        pthread_mutex_lock(&it_clients->second.send_lock);
                        memcpy(it_clients->second.send_buf, message_ptr, recvPackageLen);
                        it_clients->second.send_num += recvPackageLen;
                        pthread_mutex_unlock(&it_clients->second.send_lock);

                        free(message_ptr);
                    } else if(ret == -1 || ret == 0) {
                        if(message_ptr != NULL)
                            free(message_ptr);
//                        continue;
                    } else if(ret == 1){
                        char sendPackage[LINE_MAX];
                        memset(sendPackage, 0, LINE_MAX);
                        sprintf(sendPackage, "Client(clntfd = %d) said: ", it_clients->first);
                        memcpy(sendPackage + strlen(sendPackage), message_ptr, recvPackageLen);
                        printf("Client(clntfd = %d) said: %s\n", it_clients->first, message_ptr);
                        printf("strlen(sendPackage) = %lu\n", strlen(sendPackage));
                        int len = websocket_enPackage((unsigned char *)sendPackage, strlen(sendPackage), message_ptr, LINE_MAX, false, type);
                        if(len == -1) {
                            printf("打包出现问题\n");
                            free(message_ptr);
//                            continue;
                        }
                        auto it_client = g_clients.begin();
                        for(; it_client != g_clients.end(); ++it_client) {
                            if(it_client->second.state == CS_WEBSOCKET_CONNECTED) {
                                if(it_client->first != it_clients->first) {

                                    pthread_mutex_lock(&it_client->second.send_lock);
                                    memcpy(it_client->second.send_buf, message_ptr, len);
                                    it_client->second.send_num = len;
                                    pthread_mutex_unlock(&it_client->second.send_lock);

                                }
                            }
                        }
                        free(message_ptr);
                    }
                }
                continue;
            }
            if(it_clients->second.state == CS_CLOSED) {
                unsigned char closePackage[6];
                memset(closePackage, 0, 6);
                websocket_enPackage(NULL, 0, closePackage, 6, true, WCT_DISCONN);
//                send(it_clients->first, closePackage, 6, 0);
//                client_rw_del(it_clients->first);
                pthread_mutex_lock(&it_clients->second.send_lock);
                memset(it_clients->second.send_buf, 0, SAVE_MAX);
                memcpy(it_clients->second.send_buf, closePackage, 6);
                it_clients->second.send_num = 6;
                pthread_mutex_unlock(&it_clients->second.send_lock);
                printf("client(clntfd = %d) has been shutdown\n", it_clients->first);
                continue;
            }
        }
        pthread_mutex_unlock(&handle_lock);
        delayms(1);
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
    return (void *)0;
}

void accept_cb(int servfd, short event, void *arg) {
    printf("accept_cb start\n");
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
    pthread_mutex_lock(&handle_lock);
    printf("size = %lu\n", g_clients.size());
    if(g_clients.size() < CLIENT_MAXNUM) {
//        std::pair<std::map<int, struct client_rw>, bool> isInsert;
        g_clients.insert(std::pair<int, struct client_rw>(clntfd, *clientRw));
//        if(isInsert.second == false) {
//            printf("insert failed\n");
//            return ;
//        }
        auto it = g_clients.find(clntfd);
        if(it != g_clients.end()) {
            if(event_add(it->second.read_event, NULL) < 0) {
                printf("read_event add error: %s, errno: %d", strerror(errno), errno);
//                client_rw_free(clientRw);
                client_rw_del(clntfd);
//                close(clntfd);
                pthread_mutex_unlock(&handle_lock);
                return ;
            }
            if(event_add(it->second.write_event, NULL) < 0) {
                printf("write_event add error: %s, errno: %d", strerror(errno), errno);
//                client_rw_free(clientRw);
                client_rw_del(clntfd);
//                close(clntfd);
                pthread_mutex_unlock(&handle_lock);
                return ;
            }
        } else {
            printf("no such client which clntfd = %d\n", clntfd);
            pthread_mutex_unlock(&handle_lock);
            return ;
        }
    } else {
        printf("Client list is full\n");
        pthread_mutex_unlock(&handle_lock);
        return ;
    }
    pthread_mutex_unlock(&handle_lock);
}

void read_cb(int clntfd, short event, void *arg) {
    int ret;
    char recvPackage[LINE_MAX];
    auto it_clients = g_clients.find(clntfd);
    memset(recvPackage, 0, LINE_MAX);
    if (it_clients != g_clients.end()) {
        ret = recv(clntfd, recvPackage, LINE_MAX, 0);
        if(ret == -1) {
            printf("recv error: %s, errno: %d", strerror(errno), errno);
            if(errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN) {
                return ;
            } else {
                pthread_mutex_lock(&handle_lock);
                client_rw_del(clntfd);
                pthread_mutex_unlock(&handle_lock);
                return ;
            }
        } else if(ret == 0) {
            pthread_mutex_lock(&handle_lock);
            client_rw_del(clntfd);
            pthread_mutex_unlock(&handle_lock);
            return ;
        }

        pthread_mutex_lock(&it_clients->second.recv_lock);
        memcpy(it_clients->second.recv_buf + it_clients->second.recv_num, recvPackage, ret);
        it_clients->second.recv_num += ret;
        pthread_mutex_unlock(&it_clients->second.recv_lock);

//        printf("g_clients(clntfd = %d).recv_buf = ", it_clients->first);
//        for(j = 0; j < it_clients->second.recv_num; ++j) {
//            printf("%.2x ", it_clients->second.recv_buf[j]);
//        }
//        printf("\n");
    } else {
        printf("no such client which clntfd = %d\n", clntfd);
        close(clntfd);
        return ;
    }
}

void write_cb(int clntfd, short event, void *arg) {
    int ret, j;
    char sendPackage[LINE_MAX];
    auto it_clients = g_clients.find(clntfd);
    memset(sendPackage, 0, LINE_MAX);
    if(it_clients != g_clients.end()) {
        if(it_clients->second.send_num != 0) {
            printf("send_buf = ");
            for(j = 0; j < it_clients->second.send_num; ++j) {
                printf("%.2x ", it_clients->second.send_buf[j]);
            }
            printf("\n");
            printf("send_num = %lu\n", it_clients->second.send_num);

            pthread_mutex_lock(&it_clients->second.send_lock);
            unsigned long sendnum = it_clients->second.send_num;
            memcpy(sendPackage, it_clients->second.send_buf, it_clients->second.send_num);
            pthread_mutex_unlock(&it_clients->second.send_lock);

            ret = send(clntfd, sendPackage, sendnum, 0);
            if(ret == -1) {
                if (errno == EINTR || errno == EWOULDBLOCK || errno == EAGAIN) {
                    return;
                } else {
                    printf("send error: %s, errno: %d", strerror(errno), errno);
                    pthread_mutex_lock(&handle_lock);
                    client_rw_del(clntfd);
                    pthread_mutex_unlock(&handle_lock);
                }
            } else if(ret == 0) {
                pthread_mutex_lock(&handle_lock);
                client_rw_del(clntfd);
                pthread_mutex_unlock(&handle_lock);
                return ;
            } else if(ret < sendnum) {
                pthread_mutex_lock(&it_clients->second.send_lock);
                memcpy(it_clients->second.send_buf, sendPackage + ret, sendnum - ret);
                memset(it_clients->second.send_buf + (sendnum - ret), 0, SAVE_MAX - (sendnum - ret));
                it_clients->second.send_num -= ret;
                pthread_mutex_unlock(&it_clients->second.send_lock);
            } else if(ret == sendnum) {
                pthread_mutex_lock(&it_clients->second.send_lock);
                memset(it_clients->second.send_buf, 0, SAVE_MAX);
                it_clients->second.send_num = 0;
                pthread_mutex_unlock(&it_clients->second.send_lock);
            }
            printf("send_num(after) = %lu\n", it_clients->second.send_num);
        }
    } else {
        printf("no such client which clntfd = %d\n", clntfd);
        close(clntfd);
        return ;
    }
}

