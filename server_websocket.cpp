//
// Created by 朱宏宽 on 2020/8/11.
//

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../include/wrap.h"
#include "../include/sha1.h"
#include "../include/base64.h"
#include "../include/intLib.h"

#define REQUEST_LEN_MAX 4096
#define DEFAULT_SERVER_PORT 8000
#define WEB_SOCKET_KEY_LEN_MAX 256
#define RESPONSE_HEADER_LEN_MAX 1024
#define LINE_MAX 4096

//char *fetchSecKey(const char *buf);
int fetchSecKey(char *buf, char *ret);
char *computeAcceptKey(char *buf);
void shakeHand(int connfd, const char *s_Key);
char *analyData(const char *buf, const int bufLen);
char *packData(const char *message, unsigned long *len);
void response(int connfd, const char *message);


int main(int argc, char *argv[]) {
    struct sockaddr_in s_addr, c_addr;
    int listenfd, connfd;
    char buf[REQUEST_LEN_MAX];
    char *data;
    char str[INET_ADDRSTRLEN];
    char *secWebSocketKey;
    int port = DEFAULT_SERVER_PORT;

    if(argc > 1) {
        port = atoi(argv[1]);
    }
    if(port <= 0 || port >= 0xFFFF) {
        printf("Port(%d) is out of range(1 - %d)", port, 0xFFFF);
        exit(-1);
    }

    listenfd = Socket(AF_INET, SOCK_STREAM, 0);

    memset(&s_addr, 0, sizeof(s_addr));
    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    s_addr.sin_port = htons(port);

    unsigned int on;
    on = 0x1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, 4);

    Bind(listenfd, (sockaddr *)&s_addr, sizeof(s_addr));

    Listen(listenfd, 20);

    printf("Listen %d\nWaiting for connections ...\n", port);

    socklen_t size = sizeof(c_addr);
    connfd = Accept(listenfd, (struct sockaddr *)&c_addr, &size);
    printf("Connection from %s at port %d\n",
        inet_ntop(AF_INET, &c_addr.sin_addr, str, sizeof(str)),
        ntohs(c_addr.sin_port));

    int n;
    int connected = 0;//0: not connected, 1: connected;
    while(1) {
        memset(buf, 0, REQUEST_LEN_MAX);
        n = Read(connfd, buf, REQUEST_LEN_MAX);
        printf("--------------------\n");

        if(0 == connected) {
            printf("read:%d\n%s\n", n, buf);
            secWebSocketKey = computeAcceptKey(buf);
            shakeHand(connfd, secWebSocketKey);
            connected = 1;
            continue;
        }

        data = analyData(buf, n);
        response(connfd, data);
    }

    Close(connfd);

    return 0;
}

//char *fetchSecKey(const char *buf) {
//    char *key;
//    char *keyBegin;
//    char *flag = "Sec-WebSocket-Key: ";
//    int i = 0, bufLen = 0;
//
//    key = (char *)malloc(WEB_SOCKET_KEY_LEN_MAX);
//    memset(key, 0, WEB_SOCKET_KEY_LEN_MAX);
//    if(!buf) {
//        return NULL;
//    }
//
//    keyBegin = strstr((char *)buf, flag);
//    if(!keyBegin) {
//        return NULL;
//    }
//    keyBegin += strlen(flag);
//
//    bufLen = strlen(buf);
//    for (i = 0; i < bufLen; ++i) {
//        if(keyBegin[i] == 0x0A || keyBegin[i] == 0x0D) {
//            break;
//        }
//        key[i] = keyBegin[i];
//    }
//
//    return key;
//}

int fetchSecKey(char *buf, char *ret) {
    char *format = "Sec-WebSocket-Key: %[^\r]s";
    char *p = strstr(buf, "Sec-WebSocket-Key");

    if(p == NULL) {
#ifdef DEBUG
        printf("ERROR: get WebSocket_Key 1\n");
#endif
        return 0;
    }

    if(sscanf(p, format, ret) != 1) {
#ifdef DEBUG
        printf("ERROR: get WebSocket_Key 2\n");
#endif
        return 0;
    }

    return 1;
}

char *computeAcceptKey(char *buf) {
    char *c_Key;
    char *s_Key;
    char *sha1DataTemp;
    char *sha1Data;
    short temp;
    int i, n;
    const char *GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    if(!buf) {
        return NULL;
    }

    c_Key = (char *)malloc(LINE_MAX);
    memset(c_Key, 0, LINE_MAX);
    //c_Key = fetchSecKey(buf);
    int ret = fetchSecKey(buf, c_Key);
    if(!c_Key) {
        return NULL;
    }
    strncat(c_Key, GUID, strlen(GUID));

    sha1DataTemp = sha1_hash(c_Key);
    n = strlen(sha1DataTemp);

    sha1Data = (char *)malloc(n / 2 + 1);
    memset(sha1Data, 0, n / 2 + 1);
    for(i = 0; i < n; i += 2) {
        sha1Data[i/2] = htoi(sha1DataTemp, i, 2);
    }

    s_Key = base64_encode(sha1Data, n / 2);

    return s_Key;
}

void shakeHand(int connfd, const char *s_Key) {
    char responseHeader[RESPONSE_HEADER_LEN_MAX];

    if(!connfd) {
        return ;
    }

    if(!s_Key) {
        return ;
    }

    memset(responseHeader, '\0', RESPONSE_HEADER_LEN_MAX);

    sprintf(responseHeader, "HTTP/1.1 101 Switching Protocols\r\n");
    sprintf(responseHeader, "%sUpgrade: websocket\r\n", responseHeader);
    sprintf(responseHeader, "%sConnection: Upgrade\r\n", responseHeader);
    sprintf(responseHeader, "%sSec_WebSocket_Accept: %s\r\n\r\n", responseHeader,
            printf("Response Header: %s\n", responseHeader));
    Write(connfd, responseHeader, strlen(responseHeader));
}

char *analyData(const char *buf, const int bufLen) {
    char *data;
    char fin, maskFlag, masks[4];
    char *payloadData;
    char temp[8];
    unsigned long n, payloadLen = 0;
    unsigned short usLen = 0;
    int i = 0;

    if (bufLen < 2) {
        return NULL;
    }

    fin = ((buf[0] & 0x80) == 0x80);
    if (!fin) {
        return NULL;
    }

    maskFlag = ((buf[1] & 0x80) == 0x80);
    if(!maskFlag) {
        return NULL;
    }

    payloadLen = buf[1] & 0x7F;
    if(payloadLen == 126) {
        memcpy(masks, buf + 4, 4);
        payloadLen = (buf[2] & 0xFF) << 8 | (buf[3] & 0xFF);
        payloadData = (char *)malloc(payloadLen);
        memset(payloadData, 0, payloadLen);
        memcpy(payloadData, buf+8, payloadLen);
    } else if (payloadLen == 127) {
        memcpy(masks, buf + 10, 4);
        for (i = 0; i < 8; ++i) {
            temp[i] = buf[9 - i];
        }

        memcpy(&n, temp, 8);
        payloadData = (char *)malloc(n);
        memset(payloadData, 0, n);

        memcpy(payloadData, buf + 14, n);
        payloadLen = n;
    } else {
        memcpy(masks, buf + 2, 4);
        payloadData = (char *)malloc(payloadLen);
        memset(payloadData, 0, payloadLen);
        memcpy(payloadData, buf + 6, payloadLen);
    }

    for(i = 0; i < payloadLen; ++i) {
        payloadData[i] = (char)(payloadData[i] ^ masks[i % 4]);
    }

    printf("data(%d):%s\n", payloadLen, payloadData);
    return payloadData;
}

char *packData(const char *message, unsigned long *len) {
    char *data = NULL;
    unsigned long n;

    n = strlen(message);
    if(n < 126) {
        data = (char *)malloc(n + 2);
        memset(data, 0, n + 2);
        data[0] = 0x81;
        data[1] = n;
        memcpy(data + 4, message, n);
        *len = n + 4;
    } else {
        *len = 0;
    }

    return data;
}

void response(int connfd, const char *message) {
    char *data;
    unsigned long n = 0;
    int i;
    if(!connfd) {
        return ;
    }

    if(!data) {
        return ;
    }
    data = packData(message, &n);

    if(!data || n<= 0) {
        printf("data is empty!\n");
        return ;
    }

    Write(connfd, data, n);
}
