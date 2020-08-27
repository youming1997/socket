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

void websocket_getRandomString(unsigned char *buf, unsigned int len) {
    unsigned int i;
    unsigned char temp;
    srand(time(0));

    for(i = 0; i < len; ++i) {
        temp = (unsigned char)rand()%256;
        buf[i] = temp;
    }
}

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
            *package++ = (char)(((~temp1)&temp2) | (temp1&(~temp2)));
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

WebSocket_CommunicationType websocket_isType(unsigned char *data, unsigned int dataLen) {
    char type;
    type = data[0] & 0x0F;

    if(data[0] & 0x80) {
        switch (type) {
            case 0x01:
                return WCT_TXTDATA;
            case 0x02:
                return WCT_BINDATA;
            case 0x08:
                return WCT_DISCONN;
            case 0x09:
                return WCT_PING;
            case 0x0A:
                return WCT_PONG;
            default:
                return WCT_ERR;
        }
    } else if(type == 0x00) {
        return WCT_MINDATA;
    } else
        return WCT_ERR;
}

int websocket_isMask(unsigned char *data, unsigned int dataLen) {
    int mask = 0;

    if(data[1] & 0x80 == 0x80)
        mask = 1;

    return mask;
}

/**
 *
 * @param data //数据包
 * @param dataLen //数据包长度
 * @param maskKey //掩码
 * @param dataStart //数据起始点
 * @return len //数据长度
 */
int websocket_isDataLen(unsigned char *data, unsigned int dataLen, int isMask, unsigned char *maskKey, int *dataStart, int *payLoadLen) {
    int len;
    int start;
    int count;

    if(isMask) {
        count = MASK_LEN;
    }

    len = data[1] & 0x7F;
    payLoadLen = &len;

    if(len == 126) {
        if(dataLen < 4)
            return -1;
        len = data[2];
        len = (len << 8) + data[3];
        if(isMask) {
            maskKey[0] = data[4];
            maskKey[1] = data[5];
            maskKey[2] = data[6];
            maskKey[3] = data[7];
            start = 8;
        } else
            start = 4;
    } else if(len == 127) {
        if(dataLen < 10)
            return -1;
        if(data[2] != 0 && data[3] != 0 && data[4] != 0 && data[5] != 0)
            return -1;
        len = data[6];
        len = (len << 8) + data[7];
        len = (len << 8) + data[8];
        len = (len << 8) + data[9];
        if(isMask) {
            maskKey[0] = data[10];
            maskKey[1] = data[11];
            maskKey[2] = data[12];
            maskKey[3] = data[13];
            start = 14;
        } else
            start = 10;
    } else {
        if(dataLen < len + 2 + count)
            return -1;
        if(isMask) {
            maskKey[0] = data[2];
            maskKey[1] = data[3];
            maskKey[2] = data[4];
            maskKey[3] = data[5];
            start = 6;
        } else
            start = 2;
    }

    dataStart = &start;
    return len;
}

int websocket_dePackage(unsigned char *data, unsigned int dataLen, unsigned char *package, unsigned int packageMaxLen, unsigned int *packageLen)
{
    unsigned char maskKey[4] = {0};    // 掩码
    unsigned char temp1, temp2;
    char Mask = 0, type;
    int count, ret;
    int i, len = 0, dataStart = 2;
    int isMask, payLoadLen;
    if(dataLen < 2)
        return -1;

    //printf("data\n%s\n", data);
    ret = websocket_isType(data, dataLen);
    isMask = websocket_isMask(data, dataLen);
    len = websocket_isDataLen(data, dataLen, isMask, maskKey, &dataStart, &payLoadLen);
    //
    if(dataLen < len + dataStart)
        return WCT_ERR;
    //
    if(packageMaxLen < len + 1)
        return WCT_ERR;
    //
    if(Mask)    // 解包数据使用掩码时, 使用异或解码, maskKey[4]依次和数据异或运算, 逻辑如下
    {
        //printf("depackage : len/%d\r\n", len);
        for(i = 0; i < len; i++)
        {
            temp1 = maskKey[i % 4];
            temp2 = data[i + dataStart];
            *package++ =  (char)(((~temp1)&temp2) | (temp1&(~temp2)));  // 异或运算后得到数据
        }
        *package = '\0';
    }
    else    // 解包数据没使用掩码, 直接复制数据段
    {
        memcpy(package, &data[dataStart], len);
        package[len] = '\0';
    }
    *packageLen = len;
    //
    return ret;
}

void delayms(int ms) {
    struct timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = ms % 1000 * 1000;
    select(0, NULL, NULL, NULL, &tv);
}

int readLine(char *buf, int level, char *line) {
    int len = strlen(buf);
    for(;level < len; ++level) {
        if(buf[level] == '\r' && buf[level + 1] == '\n')
            return level + 2;
        else
            *line++ = buf[level];
    }
    return -1;
}

char * websocket_serverLinkToClient(int clntfd, char *head, unsigned int bufLen) {
    int level = 0, ret;
    char *responsePackage, sha1Data[SHA_DIGEST_LENGTH];
    char line[1024];
    unsigned char *responseKey;
    const char * GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    printf("clntfd = %d\n", clntfd);
    responsePackage = (char *)malloc(sizeof(char) * 1024);
    responseKey = (unsigned char *)malloc(sizeof(char) * 1024);
    do {
        memset(line, 0, 1024);
        level = readLine(head, level, line);
        printf("line: %s\n", line);

        if(strstr(line, "Sec-WebSocket-Key: ") != NULL) {
            strcat(line, GUID);

            SHA1((unsigned char *)(line + 19), strlen(line + 19), (unsigned char *)sha1Data);
            //sha1Data = sha1_hash(line);
            base64_encode(sha1Data, strlen(sha1Data), (char *)responseKey);

            sprintf(responsePackage, "HTTP/1.1 101 Switching Protocols\r\n"
                                     "Upgrade: websocket\r\n"
                                     "Connection: Upgrade\r\n"
                                     "Sec-WebSocket-Accept: %s\r\n\r\n", responseKey);
            ret = send(clntfd, responsePackage, strlen(responsePackage), 0);
            if(ret < 0) {
                perror("send error");
            }
            break;
        }
    } while (head[level] != '\r' || head[level + 1] != '\n');
    printf("responsepackage\n");
    printf("%s\n", responsePackage);

    free(responseKey);
    return responsePackage;
}