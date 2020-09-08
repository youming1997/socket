#include <cstdio>
#include <sys/socket.h>
#include <cstdlib>
#include <cerrno>
#include <unistd.h>
#include <map>

#include "Client.h"
#include "websocket_common.h"

#define PORT 8000
#define CLIENT_MAXNUM 100

void accept_cb(int servfd, short event, void *arg);
void read_cb(int clntfd, short event, void *arg);
void write_cb(int clntfd, short event, void *arg);
void *handle_thread(void *arg);

std::map<int, Client *> g_clients;
socklen_t socklen = sizeof(struct sockaddr_in);
pthread_mutex_t handle_lock;
int g_flag = 1;

int main() {
    int servfd;
    struct sockaddr_in s_addr;

    int ret;

    if(0 != pthread_mutex_init(&handle_lock, NULL)) {
        printf("handle_lock init error\n");
        exit(1);
    }
    servfd = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == servfd) {
        printf("socket error: %s\n", strerror(errno));
        exit(1);
    }
    printf("socket OK\n");

    evutil_make_listen_socket_reuseable(servfd);

    memset(&s_addr, 0, socklen);
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(PORT);
    s_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    ret = bind(servfd, (struct sockaddr *)&s_addr, socklen);
    if(-1 == ret) {
        printf("bind error: %s\n", strerror(errno));
        exit(1);
    }
    printf("bind OK\n");

    evutil_make_socket_nonblocking(servfd);

    ret = listen(servfd, CLIENT_MAXNUM);
    if(-1 == ret) {
        printf("listen error: %s\n", strerror(errno));
        close(servfd);
        exit(1);
    }
    printf("listen OK\n");

    event_base *base = event_base_new();
    struct event *accept_event = event_new(base, servfd, EV_READ | EV_PERSIST, accept_cb, base);
//    struct timeval tv;
//    tv.tv_sec = 24 * 60 * 60;
//    tv.tv_usec = 0;
    if(-1 == event_add(accept_event, NULL)) {
        printf("accept_event error: %s\n", strerror(errno));
        close(servfd);
        exit(1);
    }
    printf("accept_event create OK\n");

    pthread_t workpid;
    pthread_create(&workpid, NULL, handle_thread, (void *)base);
    event_base_dispatch(base);

    g_flag = 0;
    event_base_free(base);
    return 0;
}

void client_del(std::map<int, Client *>::iterator it) {
    delete it->second;
    close(it->first);
    g_clients.erase(it++);
}

void *handle_thread(void *arg) {
    auto it_client = g_clients.begin();
    auto it_sendclient = g_clients.begin();
    char data[LINE_MAX];

    while(g_flag) {
        pthread_mutex_lock(&handle_lock);
        for(it_client = g_clients.begin() ;it_client != g_clients.end(); ++it_client) {
            int ret;

            memset(data, 0, LINE_MAX);
            ret = checkState(it_client->second, data);
            if(ret == 0)
                continue;
            else if(ret == 1) {
                if(getType(it_client->second->send_buf) == WCT_PONG) {
                    it_client->second->lockSend();
                    memcpy(it_sendclient->second->send_buf, data, strlen(data));
                    it_client->second->setSendNum(strlen(data));
                    it_client->second->unlockSend();
                } else{
                    it_client->second->setState(CS_WEBSOCKET_CONNECTED);
                }
                continue;
            } else if(ret > 1) {
                for(it_sendclient = g_clients.begin(); it_sendclient != g_clients.end(); ++it_sendclient) {
                    if(it_sendclient->second->getState() == CS_WEBSOCKET_CONNECTED) {
                        it_sendclient->second->lockSend();
                        memcpy(it_sendclient->second->send_buf, data, ret);
                        it_sendclient->second->setSendNum(ret);
                        it_sendclient->second->unlockSend();
                    }
                }
                continue;
            } else if(ret == -1){
                memset(data, 0, LINE_MAX);
                enpackage(NULL, 0, WCT_DISCONN, true, data, 6);
                it_client->second->lockSend();
                memcpy(it_client->second->send_buf, data, 6);
                it_client->second->setSendNum(6);
                it_client->second->unlockSend();
//                client_del(it_client);
                continue;
            }
        }
        pthread_mutex_unlock(&handle_lock);
        delayms(1);
    }

    return (void *)0;
}

void accept_cb(int servfd, short event, void *arg) {
    int clntfd;
    struct sockaddr_in c_addr;
    struct event_base *base = (struct event_base *)arg;

    memset(&c_addr, 0, socklen);
    clntfd = accept(servfd, (struct sockaddr *)&c_addr, &socklen);
    if(-1 == clntfd) {
        printf("accept error: %s\n", strerror(errno));
    }
    printf("accept OK, clntfd = %d\n", clntfd);

    Client *client = new Client(c_addr);
    printf("client address = %p", client);
    client->recv_event = event_new(base, clntfd, EV_READ | EV_PERSIST, read_cb, client);
    printf("recv_event address = %p\n", client->recv_event);
    if(client->recv_event == NULL) {
        printf("create recv_event error\n");
        delete client;
        close(clntfd);
        return ;
    }
    printf("create recv_event OK\n");

    client->send_event = event_new(base, clntfd, EV_WRITE | EV_PERSIST, write_cb, client);
    if(client->send_event == NULL) {
        printf("create send_event error\n");
        delete client;
        close(clntfd);
        return ;
    }
    printf("create send_event OK\n");
    
    int size = g_clients.size();
    pthread_mutex_lock(&handle_lock);

    if(g_clients.size() == CLIENT_MAXNUM) {
        printf("client list is full\n");
        delete client;
        close(clntfd);
        return ;
    }
    g_clients.insert(std::pair<int, Client *>(clntfd, client));
    auto it = g_clients.find(clntfd);
    if(it != g_clients.end()) {
        if(-1 == event_add(it->second->recv_event, NULL)) {
            printf("add recv_event error\n");
            delete client;
            close(clntfd);
            return ;
        }
        printf("add recv_event OK\n");

        if(-1 == event_add(it->second->send_event, NULL)) {
            printf("add send_event error\n");
            delete client;
            close(clntfd);
            return ;
        }
        printf("add send_event OK\n");
    }
    if(g_clients.size() != size + 1) {
        printf("insert map error\n");
        delete client;
        close(clntfd);
        return ;
    }

    pthread_mutex_unlock(&handle_lock);
    printf("insert clntfd = %d OK\n", clntfd);
}

void read_cb(int clntfd, short event, void *arg) {
    int ret;
    char recv_buffer[LINE_MAX];
    auto it = g_clients.find(clntfd);

    if(it == g_clients.end()) {
        printf("client(clntfd = %d) not found\n", clntfd);
        close(clntfd);
        return ;
    }
    memset(recv_buffer, 0, LINE_MAX);
    ret = recv(clntfd, recv_buffer, LINE_MAX, 0);
    if(ret < 0) {
        printf("recv(clntfd = %d) error: %s", clntfd, strerror(errno));
        if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return ;
        } else {
            pthread_mutex_lock(&handle_lock);
            client_del(it);
            pthread_mutex_unlock(&handle_lock);
            return ;
        }
    } else if(ret == 0) {
        printf("read_cb: client(clntfd = %d) has been shutdown\n", clntfd);
        pthread_mutex_lock(&handle_lock);
        client_del(it);
        pthread_mutex_unlock(&handle_lock);
    } else {
        printf("recv_buf\n%s\n", recv_buffer);
        it->second->lockRecv();
        memcpy(it->second->recv_buf + it->second->getRecvNum(), recv_buffer, ret);
        it->second->setRecvNum(it->second->getRecvNum() + ret);
        it->second->unlockRecv();
    }
}

void write_cb(int clntfd, short event, void *arg) {
    int ret;
    char send_buffer[LINE_MAX];
    char temp[LINE_MAX];
    auto it = g_clients.find(clntfd);

    if(it == g_clients.end()) {
        printf("client(clntfd = %d)not found\n", clntfd);
        close(clntfd);
        return ;
    }
    if(it->second->getSendNum() != 0) {

        memset(send_buffer, 0, LINE_MAX);
        it->second->lockSend();
        printf("send_buf = %s\n", it->second->send_buf);
        printf("send_num = %lu\n", it->second->getSendNum());
        memcpy(send_buffer, it->second->send_buf, it->second->getSendNum());
        ret = send(clntfd, send_buffer, it->second->getSendNum(), 0);
        it->second->unlockSend();
        if (ret < 0) {
            printf("send(clntfd = %d) error: %s", clntfd, strerror(errno));
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                return;
            } else {
                pthread_mutex_lock(&handle_lock);
                client_del(it);
                pthread_mutex_unlock(&handle_lock);
                return;
            }
        } else if (ret == 0) {
            printf("send_cb: client(clntfd = %d) has been shutdown\n", clntfd);
            pthread_mutex_lock(&handle_lock);
            client_del(it);
            pthread_mutex_unlock(&handle_lock);
        } else {
            memset(temp, 0, LINE_MAX);
            it->second->lockSend();
            if (it->second->getSendNum() - ret == 0) {
                memset(it->second->send_buf, 0, it->second->getSendNum());
                it->second->setSendNum(0);
            } else {
                memcpy(temp, it->second->send_buf + ret, it->second->getSendNum() - ret);
                memcpy(it->second->send_buf, temp, it->second->getSendNum() - ret);
                memset(it->second->send_buf + it->second->getSendNum() - ret, 0, ret);
                it->second->setSendNum(it->second->getSendNum() - ret);
            }
            it->second->unlockSend();
        }
    } else
        return ;
}
