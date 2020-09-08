//
// Created by 朱宏宽 on 2020/9/6.
//

#ifndef WEBSOCKET_SERVER_PTHREAD_CLIENT_H
#define WEBSOCKET_SERVER_PTHREAD_CLIENT_H

#include <cstring>
#include <climits>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <event2/event.h>

#define SAVE_MAX 16 * 1024

enum client_state {
    CS_TCP_CONNECTED,
    CS_WEBSOCKET_CONNECTED,
    CS_CLOSE
};

class Client {
private:
    struct sockaddr_in c_addr;
    unsigned long recv_num;
    unsigned long send_num;
    pthread_mutex_t recv_lock;
    pthread_mutex_t send_lock;
    client_state state;

public:
    char recv_buf[SAVE_MAX];
    char send_buf[SAVE_MAX];
    struct event *recv_event;
    struct event *send_event;

    Client(struct sockaddr_in client_addr);

    void lockRecv();
    void unlockRecv();
    void lockSend();
    void unlockSend();
    void setRecvNum(unsigned long recvNum);
    unsigned long getRecvNum() const;
    void setSendNum(unsigned long sendNum);
    unsigned long getSendNum() const;
    void setState(client_state state);
    client_state getState();
    struct sockaddr_in getClientAddress();

    ~Client();
};

#endif //WEBSOCKET_SERVER_PTHREAD_CLIENT_H
