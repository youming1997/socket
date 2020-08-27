//
// Created by 朱宏宽 on 2020/8/13.
//

#ifndef SOCKET_SERVER_CLIENT_WEBSOCKET_COMMON_H
#define SOCKET_SERVER_CLIENT_WEBSOCKET_COMMON_H

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

//
// Created by 朱宏宽 on 2020/8/13.
//
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/un.h>
//#include <sys/epoll.h>

#include <pthread.h>

//void websocket_getRandomString(unsigned char *buf, unsigned int len);
//int websocket_getShakeKey(unsigned char *key);
//int websocket_getResponseShakeKey(unsigned char *acceptKey, unsigned char *responseKey, unsigned int acceptLen);
//unsigned char *websocket_getResponseShakeKey(unsigned char *acceptKey, unsigned int acceptLen);
//int websocket_matchShakeKey(unsigned char* clientKey, unsigned int clientKeyLen, unsigned char *responseKey, unsigned int responseKeyLen);
//void websocket_buildHttpHead(char *ip, int port, char *interfacePath, unsigned char *shakeKey, char *package);
//char * websocket_buildHttpResponse(unsigned char *acceptKey, unsigned int acceptKeyLen, char *package);
#define MASK_LEN 4
WebSocket_CommunicationType websocket_isType(unsigned char *data, unsigned int dataLen);
int websocket_isMask(unsigned char *data, unsigned int dataLen);
int websocket_isDataLen(unsigned char *data, unsigned int dataLen, int isMask, unsigned char *maskKey, int *dataStart, int *payLoadLen);
int websocket_enPackage(unsigned char *data, unsigned int dataLen, unsigned char *package, unsigned int packageMaxLen, bool isMask, WebSocket_CommunicationType type);
int websocket_dePackage(unsigned char *data, unsigned int dataLen, unsigned char *package, unsigned int packageMaxLen, unsigned int *packageLen);
int setNonblocking(int fd);
void delayms(int ms);
//int websocket_clientLinkToServer(char *ip, int port, char *interface_path);
char * websocket_serverLinkToClient(int clntfd, char *recvBuf, unsigned int bufLen);
int websocket_send(int fd, unsigned char *data, unsigned int dataLen, bool mod, WebSocket_CommunicationType type);
int websocket_recv(int fd, unsigned char *data, unsigned int dataMaxLen);
#endif //SOCKET_SERVER_CLIENT_WEBSOCKET_COMMON_H
