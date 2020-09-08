//
// Created by 朱宏宽 on 2020/9/6.
//

#include "Client.h"

Client::Client(struct sockaddr_in client_addr) {
    this->c_addr = client_addr;
    memset(this->recv_buf, 0, SAVE_MAX);
    memset(this->send_buf, 0, SAVE_MAX);
    this->recv_num = 0;
    this->send_num = 0;
    pthread_mutex_init(&this->recv_lock, NULL);
    pthread_mutex_init(&this->send_lock, NULL);
    this->state = CS_TCP_CONNECTED;

}

void Client::lockRecv() {
    if(pthread_mutex_trylock(&recv_lock) != 0) {
        printf("recv_lock already locked\n");
    }
}

void Client::unlockRecv() {
    if(pthread_mutex_trylock(&recv_lock) == 0) {
        printf("recv_lock no need to unlock\n");
    }
    pthread_mutex_unlock(&recv_lock);
}

void Client::lockSend() {
    if(pthread_mutex_trylock(&send_lock) != 0) {
        printf("recv_lock already locked\n");
    }
}

void Client::unlockSend() {
    if(pthread_mutex_trylock(&send_lock) == 0) {
        printf("send_lock no need to unlock\n");
    }
    pthread_mutex_unlock(&send_lock);
}

void Client::setRecvNum(unsigned long recvNum) {
    this->recv_num = recvNum;
}

unsigned long Client::getRecvNum() const {
    return this->recv_num;
}

void Client::setSendNum(unsigned long sendNum) {
    this->send_num = sendNum;
}

unsigned long Client::getSendNum() const {
    return this->send_num;
}

void Client::setState(client_state state) {
    this->state = state;
}

client_state Client::getState() {
    return state;
}

struct sockaddr_in Client::getClientAddress() {
    return this->c_addr;
}

Client::~Client() {
    pthread_mutex_trylock(&this->recv_lock);
    pthread_mutex_unlock(&this->recv_lock);
    pthread_mutex_destroy(&this->recv_lock);
    
    pthread_mutex_trylock(&this->send_lock);
    pthread_mutex_unlock(&this->send_lock);
    pthread_mutex_destroy(&this->recv_lock);
    
    if(this->recv_event != NULL)
        event_free(this->recv_event);
    if(this->send_event != NULL)
        event_free(this->send_event);
    memset(this->recv_buf, 0, SAVE_MAX);
    memset(this->send_buf, 0, SAVE_MAX);
    
}
