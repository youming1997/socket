#ifndef NEW_WEBSOCKET_SERVER_WEBSOCKET_COMMON_H
#define NEW_WEBSOCKET_SERVER_WEBSOCKET_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <limits.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/un.h>
#include <event2/event.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/pem.h>

#include <pthread.h>

#define SAVE_MAX 16 * 1024
#define MASK_LEN 4
#define DP_SHORT "Not long enough.\n"
#define DP_PONG "This is a pong package.\n"
#define DP_ERR "Wrong type.\n"

typedef enum {
    WCT_MINDATA = -20,      // 0x0：标识一个中间数据包
    WCT_TXTDATA = -19,      // 0x1：标识一个txt类型数据包
    WCT_BINDATA = -18,      // 0x2：标识一个bin类型数据包
    WCT_DISCONN = -17,      // 0x8：标识一个断开连接类型数据包
    WCT_PING = -16,         // 0x9：表示一个ping类型数据包
    WCT_PONG = -15,         // 0xA：表示一个pong类型数据包
    WCT_ERR = -1,
    WCT_NULL = 0
}WebSocket_CommunicationType;

enum client_state {
    CS_TCP_CONNECTED,                   //TCP连接成功
    //CS_TCP_FAILED,                      //TCP连接失败
    CS_WEBSOCKET_CONNECTED,             //websocket连接成功
    //CS_WEBSOCKET_FAILED,                //websocket连接失败
    //CS_SENDING,                         //发送消息中
    //CS_HANDLING,                        //数据包处理中
    //CS_OVERTIME,                        //超时
    CS_CLOSED,                          //已关闭
};

struct client_rw {
    int clntfd;                         //客户端句柄
    struct sockaddr_in clnt_addr;       //客户端相关信息
    struct event *read_event;           //从客户端读数据的方法
    struct event *write_event;          //向客户端写数据的方法
    unsigned char recv_buf[SAVE_MAX];   //从客户端读的数据
    unsigned char send_buf[SAVE_MAX];   //向客户端写的数据
    unsigned long recv_num;             //读到的数据的长度
    unsigned long send_num;             //写出的数据的长度
    pthread_mutex_t recv_lock;          //读锁
    pthread_mutex_t send_lock;          //写锁
    pthread_mutex_t handle_lock;        //数据处理锁
    enum client_state state;            //客户端状态
};

int websocket_enPackage(unsigned char *data, unsigned int dataLen, unsigned char *package, unsigned int packageMaxLen, bool isMask, WebSocket_CommunicationType type);
WebSocket_CommunicationType websocket_getType(unsigned char *package, unsigned int packageLen);
int websocket_isMask(unsigned char *package, unsigned int dataLen);
int websocket_getDataLen(unsigned char *package, unsigned int packageLen, int isMask, unsigned char *maskKey, int *dataStart, int *payLoadLen);
char *websocket_dePackage(unsigned char *package, unsigned int packageLen, int isMask, char *maskKey, int dataStart, unsigned int dataLen);
char * websocket_serverLinkToClient(int clntfd, char *head, unsigned int bufLen);
void delayms(int ms);
int websocket_getHead(struct client_rw *clientRw);
char *websocket_getRecvPackage(struct client_rw *clientRw, unsigned int *recvPackageLen);

#endif //NEW_WEBSOCKET_SERVER_WEBSOCKET_COMMON_H
