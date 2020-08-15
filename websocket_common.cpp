//
// Created by 朱宏宽 on 2020/8/13.
//

#include "websocket_common.h"

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
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/pem.h>

#include <pthread.h>

#include "wrap.h"

int base64_encode( const char *indata, int in_len, char *outdata) {
    BIO *b64, *bio;
    BUF_MEM *bptr = NULL;
    size_t size = 0;

    if(indata == NULL || outdata == NULL) {
        return -1;
    }

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_write(bio, indata, in_len);
    BIO_flush(bio);

    BIO_get_mem_ptr(bio, &bptr);
    memcpy(outdata, bptr->data, bptr->length);
    outdata[bptr->length - 1] = '\0';
    size = bptr->length;

    BIO_free_all(bio);
    return size;
}

#define REPORT_LOGIN_CONNECT_TIMEOUT 1000
#define REPORT_LOGIN_RESPOND_TIMEOUT (1000 + REPORT_LOGIN_CONNECT_TIMEOUT)

#define REPORT_ANALYSIS_ERR_RESEND_DELAY 500

#define WEBSOCKET_SHAKE_KEY_LEN 16

void websocket_getRandomString(unsigned char *buf, unsigned int len) {
    unsigned int i;
    unsigned char temp;
    srand(time(0));

    for(i = 0; i < len; ++i) {
        temp = (unsigned char)rand()%256;
        buf[i] = temp;
    }
}

int websocket_getResponseShakeKey(unsigned char *acceptKey, unsigned char *responseKey, unsigned int acceptLen) {
    char *clientKey;
    char *sha1Data;
    int n;
    const char * GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    unsigned int GUIDLen;

    if (acceptKey== NULL) {
        return 0;
    }

    GUIDLen = sizeof(GUID);
    clientKey = (char *)malloc(sizeof(char) * (acceptLen + GUIDLen + 10));
    memset(clientKey, 0, (acceptLen + GUIDLen + 10));
    memcpy(clientKey, acceptKey, acceptLen);
    memcpy(&clientKey[acceptLen], GUID, GUIDLen);
    SHA1((unsigned char *)clientKey, strlen(clientKey), (unsigned char *)sha1Data);
    n = base64_encode(sha1Data, strlen(sha1Data), (char *)responseKey);
    free(sha1Data);
    free(clientKey);

    return n;
}

int websocket_matchShakeKey(unsigned char* clientKey, unsigned int clientKeyLen, unsigned char *responseKey, unsigned int responseKeyLen) {
    int retLen;
    unsigned char tempKey[256] = {0};

    retLen = websocket_getResponseShakeKey(clientKey, tempKey, clientKeyLen);

    if(retLen != responseKeyLen) {
        printf("websocket_matchShakeKey: len err\r\n%s\r\n%s\r\n%s\r\n", clientKey, tempKey, responseKey);
        return -1;
    } else if(strcmp((const char *)tempKey, (const char *)responseKey) != 0) {
        printf("websocket_matchShakeKey: str err\r\n%s\r\n%s\r\n", tempKey, responseKey);
        return -1;
    }

    return 0;
}

void websocket_buildHttpHead(char *ip, int port, char *interfacePath, unsigned char *shakeKey, char *package) {
    const char httpDemo[] = "GET %s HTTP/1.1\r\n"
                            "Connection: Upgrade\r\n"
                            "Host: %s:%d\r\n"
                            "Sec-WebSocket-Key: %s\r\n"
                            "Sec-WebSocket-Version: 13\r\n"
                            "Upgrade: websocket\r\n\r\n";

    sprintf(package, httpDemo, interfacePath, ip, port, shakeKey);
}

void websocket_buildHttpResponse(unsigned char *acceptKey, unsigned int acceptKeyLen, char *package) {
    const char httpDemo[] = "HTTP/1.1 101 Switching Protocols\r\n"
                            "Upgrade: websocket\r\n"
                            "Server: Microsoft-HTTPAPI/2.0\r\n"
                            "Connection: Upgrade\r\n"
                            "Sec-WebSocket-Accept: %s\r\n";
    unsigned char responseShakeKey[256] = {0};
    websocket_getResponseShakeKey(acceptKey, responseShakeKey, acceptKeyLen);
    sprintf(package, httpDemo, responseShakeKey);
    printf("%s", package);
}

#define MASK_LEN 4

int websocket_enPackage(unsigned char *data, unsigned int dataLen, unsigned char *package, unsigned int packageMaxLen, bool isMask, WebSocket_CommunicationType type) {
    unsigned char maskKey[MASK_LEN] = {0};
    unsigned char temp1, temp2;
    unsigned int i, len = 0;

    if (packageMaxLen < 2) {
        return -1;
    }

    switch (type) {
        case WCT_MINDATA:
            *package++ = 0x00;
            break;
        case WCT_TXTDATA:
            *package++ = 0x81;
            break;
        case WCT_BINDATA:
            *package++ = 0x82;
            break;
        case WCT_DISCONN:
            *package++ = 0x88;
            break;
        case WCT_PING:
            *package++ = 0x89;
            break;
        case WCT_PONG:
            *package++ = 0x8A;
            break;
        default:
            return -1;
    }

    if(isMask) {
        *package = 0x80;
    }
    len += 1;

    if(dataLen < 126) {
        *package++ |= (dataLen & 0x7F);
        len += 1;
    } else if(dataLen < 65535) {
        if(packageMaxLen < 4)
            return -1;
        *package++ |= 0x7E;
        *package++ = (char)((dataLen >> 8) & 0xFF);
        *package++ = (unsigned char)((dataLen >> 0) & 0xFF);
        len += 3;
    } else if(dataLen < 0xFFFFFFFF) {
        if(packageMaxLen < 10) {
            return -1;
        }
        *package++ |= 0x7F;
        *package++ = 0;
        *package++ = 0;
        *package++ = 0;
        *package++ = 0;
        *package++ = (char)((dataLen >> 24) & 0xFF);
        *package++ = (char)((dataLen >> 16) & 0xFF);
        *package++ = (char)((dataLen >> 8) & 0xFF);
        *package++ = (char)((dataLen >> 0) & 0xFF);
        len += 9;
    }

    if(isMask) {
        if(packageMaxLen < len + dataLen + MASK_LEN) {
            return -1;
        }
        websocket_getRandomString(maskKey, sizeof(maskKey));
        *package++ = maskKey[0];
        *package++ = maskKey[1];
        *package++ = maskKey[2];
        *package++ = maskKey[3];
        len += MASK_LEN;

        for(i = 0; i < dataLen; ++i) {
            temp1 = maskKey[i % sizeof(maskKey)];
            temp2 = data[i];
            *package++ = (char)(temp1 ^ temp2);
        }
        len += dataLen;
    } else {
        if(packageMaxLen < len + dataLen) {
            return -1;
        }
        memcpy(package, data, dataLen);
        package[dataLen] = '\0';
        len += dataLen;
    }

    return len;
}

int websocket_dePackage(unsigned char *data, unsigned int dataLen, unsigned char *package, unsigned int packageMaxLen, unsigned int *packageLen) {
    unsigned char maskKey[MASK_LEN] = {0};
    unsigned char temp1, temp2;
    char Mask = 0, type;
    int count, ret;
    unsigned int i, len = 0, dataStart = 2;

    if(dataLen < 2) {
        return -1;
    }
    type = data[0] & 0x0F;

    if((data[0] & 0x80) == 0x80) {
        switch (type) {
            case 0x01:
                ret = WCT_TXTDATA;
                break;
            case 0x02:
                ret = WCT_BINDATA;
                break;
            case 0x08:
                ret = WCT_DISCONN;
                break;
            case 0x09:
                ret = WCT_PING;
                break;
            case 0x0A:
                ret = WCT_PONG;
                break;
            default:
                return WCT_ERR;
        }
    } else if(type == 0x00){
        ret = WCT_MINDATA;
    } else
        return WCT_ERR;

    if((data[1] & 0x80) == 0x80) {
        Mask = 1;
        count = MASK_LEN;
    } else {
        Mask = 0;
        count = 0;
    }

    len = data[1] & 0x7F;

    if(len == 126) {
        if(dataLen < 4) {
            return WCT_ERR;
        }
        len = data[2];
        len = (len << 8) + data[3];

        if(dataLen < len + 4 + count) {
            return WCT_ERR;
        }

        if(Mask) {
            maskKey[0] = data[4];
            maskKey[1] = data[5];
            maskKey[2] = data[6];
            maskKey[3] = data[7];
            dataStart = 8;
        } else
            dataStart = 4;
    } else if(len == 127) {
        if(dataLen < 10) {
            return WCT_ERR;
        }
        if(data[2] != 0 || data[3] != 0 || data[4] != 0 || data[5] != 0) {
            return WCT_ERR;
        }
        len = data[6];
        len = (len << 8) + data[7];
        len = (len << 8) + data[8];
        len = (len << 8) + data[9];
        if(dataLen < len + 10 + count) {
            return WCT_ERR;
        }
        if(Mask) {
            maskKey[0] = data[10];
            maskKey[0] = data[11];
            maskKey[0] = data[12];
            maskKey[0] = data[13];
            dataStart = 14;
        } else
            dataStart = 10;
    } else {
        if(dataLen < len + 2 + count) {
            return WCT_ERR;
        }
        if(Mask) {
            maskKey[0] = data[2];
            maskKey[0] = data[3];
            maskKey[0] = data[4];
            maskKey[0] = data[5];
            dataStart = 6;
        } else
            dataStart = 2;
    }

    if(dataLen < len + dataStart) {
        return WCT_ERR;
    }

    if(packageMaxLen < len + 1) {
        return WCT_ERR;
    }

    if(Mask) {
        for(i = 0; i < len; ++i) {
            temp1 = maskKey[i % MASK_LEN];
            temp2 = data[i + dataStart];
            *package++ = (char)(temp1 ^ temp2);
        }
        *package = '\0';
    } else {
        memcpy(package, &data[dataStart], len);
        package[len] = '\0';
    }

    *packageLen = len;

    return ret;
}

int setNonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, old_option | O_NONBLOCK);

    return old_option;
}

void delayms(int ms) {
    struct timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = ms % 1000 * 1000;
    select(0, NULL, NULL, NULL, &tv);
}

int readline(char* allbuf,int level,char* linebuf)
{
    int len = strlen(allbuf);
    for (;level<len;++level)
    {
        if(allbuf[level]=='\r' && allbuf[level+1]=='\n')
            return level+2;
        else
            *(linebuf++) = allbuf[level];
    }
    return -1;
}

//int websocket_clientLinkToServer(char *ip, int port, char *interface_path) {
//    int old_option, servfd, timeout, ret;
//    unsigned char loginBuf[512], recvBuf[512], shakeKey[128];
//    unsigned char *p;
//    struct sockaddr_in s_addr;

//    memset(loginBuf, 0, 512);
//    memset(recvBuf, 0, 512);
//    memset(shakeKey, 0, 128);
//    memset(&s_addr, 0, sizeof(s_addr));

//    s_addr.sin_family = AF_INET;
//    s_addr.sin_addr.s_addr = inet_addr(ip);
//    s_addr.sin_port = htons(port);

//    servfd = Socket(AF_INET, SOCK_STREAM, 0);

//    old_option = setNonblocking(servfd);

//    timeout = 0;
//    while (-1 == connect(servfd, (struct sockaddr *)&s_addr, sizeof(struct sockaddr))) {
//        if(++timeout > REPORT_LOGIN_CONNECT_TIMEOUT) {
//            printf("websocket_login error:%s:%d cannot connect!  %d\r\n", ip, port, timeout);
//            sprintf((char *)loginBuf, "websocket_login error:%s:%d cannot connect!  %d\r\n", ip, port, timeout);
//            close(servfd);
//            return -timeout;
//        }
//        delayms(1);
//    }

//    websocket_getShakeKey(shakeKey);
//    websocket_buildHttpHead(ip, port, interface_path, shakeKey, (char *)loginBuf);

    //MAG_NOSIGNAL为Linux下的
//    ret = send(servfd, loginBuf, sizeof(loginBuf), MSG_NOSIGNAL);

//    while(1) {
//        memset(recvBuf, 0, sizeof(recvBuf));
//        ret = recv(servfd, recvBuf, sizeof(recvBuf), MSG_NOSIGNAL);
//        if(ret > 0) {
//            if(strncmp((const char *)recvBuf, (const char *)"HTTP", strlen((const char *)"HTTP")) == 0) {
//                if(NULL != (p = (unsigned char *)strstr((const char *)recvBuf, "Sec-WebSocket-Accept: "))) {
//                    p += strlen((const char *)"Sec-WebSocket-Accept: ");
//                    sscanf((const char *)p, "%s\r\n", p);
//                    if(websocket_matchShakeKey(shakeKey, strlen((const char *)shakeKey), p, strlen((const char *)p)) == 0) {
//                        return servfd;
//                    } else {
//                        ret = send(servfd, loginBuf, sizeof(loginBuf), MSG_NOSIGNAL);
//                    }
//                } else {
//                    ret = send(servfd, loginBuf, sizeof(loginBuf), MSG_NOSIGNAL);
//                }
//            }
//        } else if(ret <= 0){
//            ;
//        }
//        if(++timeout > REPORT_LOGIN_RESPOND_TIMEOUT) {
//            close(servfd);
//            return -timeout;
//        }
//        delayms(1);
//    }
//
//    close(servfd);
//    return -timeout;
//}

int websocket_serverLinkToClient(int clntfd, char *recvBuf, unsigned int bufLen) {
    char *p;
    int ret;
    char recvShakeKey[512], responsePackage[1024];

    memset(recvShakeKey, 0, sizeof(recvShakeKey));
    memset(responsePackage, 0, sizeof(responsePackage));
    p = (char *)malloc(sizeof(char) * 256);
    if(NULL == (p = strstr(recvBuf, "Sec-WebSocket-Key: "))) {
        return -1;
    }
    p += strlen("Sec-WebSocket-Key: ");
    printf("p = %s", p);
    p[24] = '\r';
    p[25] = '\n';
    p[26] = '\0';
    printf("p = %s", p);
    ret = strlen(p);
    if(ret < 1) {
         return -1;
    }
    websocket_buildHttpResponse((unsigned char *)p, ret, responsePackage);
    printf("%s\n", responsePackage);
    free(p);
    return send(clntfd, responsePackage, strlen(responsePackage), MSG_NOSIGNAL);
}

int websocket_send(int fd, unsigned char *data, unsigned int dataLen, bool mod, WebSocket_CommunicationType type) {
    unsigned char *websocketPackage;
    unsigned int retLen, ret;

    websocketPackage = (unsigned char *)(malloc(sizeof(char) * (dataLen + 128)));
    memset(websocketPackage, 0, strlen((const char *)websocketPackage));
    retLen = websocket_enPackage(data, dataLen, websocketPackage, (dataLen + 128), mod, type);

    ret = send(fd, websocketPackage, retLen, MSG_NOSIGNAL);
    free(websocketPackage);
    return ret;
}

int websocket_recv(int fd, unsigned char *data, unsigned int dataMaxLen) {
    unsigned char *websocketPackage, *recvBuf;
    int ret, ret2 = 0;
    unsigned int retLen = 0;

    recvBuf = (unsigned  char *)malloc(sizeof(char) * dataMaxLen);
    memset(recvBuf, 0, dataMaxLen);
    ret = recv(fd, recvBuf, dataMaxLen, MSG_NOSIGNAL);
    if(ret > 0) {
        //连接客户端
        if(strncmp((const char *)recvBuf, "GET", 3) == 0) {
            ret2 = websocket_serverLinkToClient(fd, (char *)recvBuf, ret);
            free(recvBuf);
            if (ret2 < 0) {
                memset(data, 0, dataMaxLen);
                strcpy((char *)data, "connect failed!\r\n");
                return strlen("connect failed!\r\n");
            }
            memset(data, 0, dataMaxLen);
            strcpy((char *)data, "connect ...\r\n");
            return strlen("connect ...\r\n");
        }
        websocketPackage = (unsigned char *)malloc(sizeof(char) * (ret + 128));
        memset(websocketPackage, 0, ret + 128);
        ret2 = websocket_dePackage(data, ret, websocketPackage, ret + 128, &retLen);
        if(ret2 == WCT_PING && retLen > 0) {
            websocket_send(fd, websocketPackage, retLen, true, WCT_PONG);
            printf("webSocket recv: PING %d\r\n%s\r\n", retLen, websocketPackage);
            free(recvBuf);
            free(websocketPackage);
            return WCT_NULL;
        } else if (retLen > 0 && (ret2 == WCT_TXTDATA || ret2 == WCT_BINDATA || ret2 == WCT_MINDATA)) {
            memcpy(data, websocketPackage, retLen);

            free(recvBuf);
            free(websocketPackage);
            return retLen;
        }
        free(recvBuf);
        free(websocketPackage);
        return -ret;
    } else {
        free(recvBuf);
        return ret;
    }
}
