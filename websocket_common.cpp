//
// Created by 朱宏宽 on 2020/8/29.
//

#include "websocket_common.h"

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
        temp = (unsigned char)rand() % 256;
        buf[i] = temp;
    }
}

int websocket_enPackage(unsigned char *data, unsigned int dataLen, unsigned char *package, unsigned int packageMaxLen, bool isMask, WebSocket_CommunicationType type) {
    unsigned char maskKey[MASK_LEN] = {0};
    unsigned char temp1, temp2;
    unsigned int i, len = 0;
//    unsigned char *send = package;

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
    } else {
        *package = 0x00;
    }
    len += 1;
//    printf("package = %.2x", *package);

    if(dataLen < 126) {
        *package++ |= (dataLen & 0x7F);
        len += 1;
    } else if(dataLen < 0xFFFF) {
        if(packageMaxLen < 4)
            return -1;
        *package++ |= 0x7E;
        *package++ = (char)((dataLen >> 8) & 0xFF);
        *package++ = (char)((dataLen >> 0) & 0xFF);
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
            *package++ = (char)(((~temp1) & temp2) | (temp1 & (~temp2)));
        }
 //       len += dataLen;
    } else {
        if(packageMaxLen < len + dataLen) {
            return -1;
        }
        memcpy(package, data, dataLen);
//        package[dataLen] = '\0';
//        len += dataLen;
    }
    len += dataLen;
//    *package = *send;

    return len;
}

WebSocket_CommunicationType websocket_getType(unsigned char *package, unsigned int packageLen) {
    char type;
    type = package[0] & 0x0F;
    if (package[0] & 0x80) {
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
    }else if (type == 0x00) {
        return WCT_MINDATA;
    } else
        return WCT_ERR;
}

int websocket_isMask(unsigned char *package, unsigned int dataLen) {
    int mask = 0;

    if((package[1] & 0x80) == 0x80)
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
int websocket_getDataLen(unsigned char *package, unsigned int packageLen, int isMask, unsigned char *maskKey, int *dataStart, int *recvPackageLen) {
    int len, start, recvLen;
    int count = 0;
    recvLen = *recvPackageLen;

    if(isMask) {
        count = MASK_LEN;
    }

    len = package[1] & 0x7F;
//    *payLoadLen = len;

    if(len == 126) {
        if(packageLen < 4 + count)
            return -1;
        recvLen += 4;
        len = package[2];
        len = (len << 8) + package[3];
        if(isMask) {
            maskKey[0] = package[4];
            maskKey[1] = package[5];
            maskKey[2] = package[6];
            maskKey[3] = package[7];
            start = 8;
        } else
            start = 4;
    } else if(len == 127) {
        if(packageLen < 10 + count)
            return -1;
        recvLen += 10;
        if(package[2] != 0 && package[3] != 0 && package[4] != 0 && package[5] != 0)
            return -1;
        len = package[6];
        len = (len << 8) + package[7];
        len = (len << 8) + package[8];
        len = (len << 8) + package[9];
        if(isMask) {
            maskKey[0] = package[10];
            maskKey[1] = package[11];
            maskKey[2] = package[12];
            maskKey[3] = package[13];
            start = 14;
        } else
            start = 10;
    } else {
        if(packageLen < len + 2 + count)
            return -1;
        recvLen += 2;
        if(isMask) {
            maskKey[0] = package[2];
            maskKey[1] = package[3];
            maskKey[2] = package[4];
            maskKey[3] = package[5];
            start = 6;
        } else
            start = 2;
    }

    *recvPackageLen = recvLen;
    *dataStart = start;
    return len;
}

int websocket_dePackage(const unsigned char *package, unsigned int packageLen, unsigned char *message, int isMask, const char *maskKey, int dataStart, unsigned int dataLen) {
    char *data;
    unsigned char temp1, temp2;
    unsigned int i;

    data = (char *)malloc(sizeof(char) * (dataLen));
    //printf("data\n%s\n", data);
    //
    if(packageLen < dataLen + dataStart)
        return -1;
//    //
//    if(dataMaxLen < len + 1)
//        return WCT_ERR;
    //
    if(isMask)    // 解包数据使用掩码时, 使用异或解码, maskKey[4]依次和数据异或运算, 逻辑如下
    {
        //printf("depackage : len/%d\r\n", len);
        for(i = 0; i < dataLen; ++i)
        {
            temp1 = maskKey[i % 4];
            temp2 = package[i + dataStart];
            data[i] = (char)(((~temp1) & temp2) | (temp1 & (~temp2)));  // 异或运算后得到数据
        }
    }
    else    // 解包数据没使用掩码, 直接复制数据段
    {
        memcpy(data, &package[dataStart], dataLen);
        data[dataLen] = '\0';
    }
    //
    memcpy(message, data, dataLen);
    free(data);
    return 1;
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

char *websocket_serverLinkToClient(int clntfd, char *head, unsigned int bufLen) {
    int level = 0;
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
//        printf("line: %s\n", line);

        if(strstr(line, "Sec-WebSocket-Key: ") != NULL) {
            strcat(line, GUID);

            SHA1((unsigned char *)(line + 19), strlen(line + 19), (unsigned char *)sha1Data);
            //sha1Data = sha1_hash(line);
            base64_encode(sha1Data, strlen(sha1Data), (char *)responseKey);

            sprintf(responsePackage, "HTTP/1.1 101 Switching Protocols\r\n"
                                     "Upgrade: websocket\r\n"
                                     "Connection: Upgrade\r\n"
                                     "Sec-WebSocket-Accept: %s\r\n\r\n", responseKey);
            break;
        }
    } while (level);
//    printf("responsepackage\n");
//    printf("%s\n", responsePackage);

    free(responseKey);
    return responsePackage;
}

int websocket_getHead(struct client_rw *clientRw) {
    unsigned int headStart = 0, headEnd = 0, j;
    for(j = 0; j < clientRw->recv_num; ++j) {
        if(clientRw->recv_buf[j] == 'G' && clientRw->recv_buf[j + 1] == 'E' && clientRw->recv_buf[j + 2] == 'T')
            headStart = j;
        if(clientRw->recv_buf[j] == '\r' && clientRw->recv_buf[j + 1] == '\n' && clientRw->recv_buf[j + 2] == '\r' && clientRw->recv_buf[j + 3] == '\n')
            headEnd = j + 3;
    }
    if(headEnd != 0) {
        char head[headEnd - headStart + 1];
        char temp[LINE_MAX];
        memset(head, 0, headEnd - headStart + 1);
        printf("head create...\n");
        pthread_mutex_lock(&clientRw->recv_lock);
        memcpy(head, clientRw->recv_buf + headStart, headEnd - headStart + 1);
        pthread_mutex_unlock(&clientRw->recv_lock);

 //       printf("head\n%s", head);
        char *responsePackage = websocket_serverLinkToClient(clientRw->clntfd, head, headEnd - headStart + 1);
        
        pthread_mutex_lock(&clientRw->recv_lock);
        memcpy(temp, clientRw->recv_buf + headEnd + 1, clientRw->recv_num - headEnd);
        memcpy(clientRw->recv_buf + headStart, temp, clientRw->recv_num - headEnd);
        memset(clientRw->recv_buf + clientRw->recv_num - (headEnd - headStart + 1), 0, headEnd - headStart + 1);
        clientRw->recv_num -= headEnd - headStart + 1;
        pthread_mutex_unlock(&clientRw->recv_lock);
        
        pthread_mutex_lock(&clientRw->send_lock);
        memcpy(clientRw->send_buf, responsePackage, strlen(responsePackage));
        clientRw->send_num = strlen(responsePackage);
        pthread_mutex_unlock(&clientRw->send_lock);
        free(responsePackage);
        return 1;
    }
    return 0;
}

int websocket_getRecvPackage(struct client_rw *clientRw, unsigned char *message, unsigned int *recvPackageLen) {
    if(clientRw->recv_num != 0) {
        int isMask = 0, dataLen = 0, dataStart = 0, packageLen = 0;
        int i;
        unsigned char maskKey[MASK_LEN] = {0};
        WebSocket_CommunicationType type;
        unsigned char package[LINE_MAX];

        memset(package, 0, LINE_MAX);
        //memset(message, 0, LINE_MAX);
        type = websocket_getType(clientRw->recv_buf, clientRw->recv_num);
        if(type == WCT_BINDATA || type == WCT_TXTDATA || type == WCT_PING) {
            isMask = websocket_isMask(clientRw->recv_buf, clientRw->recv_num);
            if(isMask) {
                packageLen += MASK_LEN;
            }
            dataLen = websocket_getDataLen(clientRw->recv_buf, clientRw->recv_num, isMask, maskKey, &dataStart, &packageLen);
            if(-1 == dataLen) {
                return -1;
            }
            packageLen += dataLen;
            if(clientRw->recv_num >= packageLen) {
                unsigned char temp[LINE_MAX];

                pthread_mutex_lock(&clientRw->recv_lock);
                memcpy(package, clientRw->recv_buf, packageLen);
                memcpy(temp, clientRw->recv_buf + packageLen, clientRw->recv_num - packageLen);
                memcpy(clientRw->recv_buf, temp, clientRw->recv_num - packageLen);
                memset(clientRw->recv_buf + (clientRw->recv_num - packageLen), 0, packageLen);
                clientRw->recv_num -= packageLen;
                pthread_mutex_unlock(&clientRw->recv_lock);

                int ret = websocket_dePackage(package, packageLen, message, isMask, (char *)maskKey, dataStart, dataLen);
                if(ret == -1) {
                    return -1;
                }
                if(type == WCT_PING) {
                    memset(package, 0, LINE_MAX);
                    int pongPackageLen = websocket_enPackage((unsigned char *)message, dataLen, package, LINE_MAX, true, WCT_PONG);
                    char *pongPackage = (char *)malloc((strlen(DP_PONG) + pongPackageLen) * sizeof(char));
                    strncpy(pongPackage, DP_PONG, strlen(DP_PONG));
                    strncat(pongPackage, (char *)package, pongPackageLen);
                    *recvPackageLen = pongPackageLen;
                    memcpy(message, pongPackage, pongPackageLen);
                    return WCT_PONG;
                }
                *recvPackageLen = dataLen;
                return 1;
            } else
                return 0;
        } else if(type == WCT_DISCONN) {
            clientRw->state = CS_CLOSED;
            return -1;
        }
    }
    return -1;
}

