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

int setNonblocking(int fd);
void delayms(int ms);
int websocket_clientLinkToServer(char *ip, int port, char *interface_path);
int websocket_serverLinkToClient(int clntfd, char *recvBuf, unsigned int bufLen) ;
int websocket_send(int fd, unsigned char *data, unsigned int dataLen, bool mod, WebSocket_CommunicationType type);
int websocket_recv(int fd, unsigned char *data, unsigned int dataMaxLen);

#endif //SOCKET_SERVER_CLIENT_WEBSOCKET_COMMON_H
