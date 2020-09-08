//
// Created by 朱宏宽 on 2020/9/7.
//

#ifndef WEBSOCKET_SERVER_PTHREAD_WEBSOCKET_COMMON_H
#define WEBSOCKET_SERVER_PTHREAD_WEBSOCKET_COMMON_H

#include <cstring>
#include <ctime>
#include <cstdlib>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/pem.h>

#include "Client.h"

#define MASK_LEN 4

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

int readLine(char buf[], int level, char line[]);
WebSocket_CommunicationType getType(char buffer[]);
int enpackage(char data[], int dataLen, WebSocket_CommunicationType type, bool isMask, char package[], unsigned int packageMaxLen);
int checkState(Client *client, char data[]);
int getRequestKey(Client *client, char requestKey[]);
int getResponseHead(char requestKey[], char responseHead[]);
void delayms(int ms);

#endif //WEBSOCKET_SERVER_PTHREAD_WEBSOCKET_COMMON_H
