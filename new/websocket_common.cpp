//
// Created by 朱宏宽 on 2020/9/7.
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

int readLine(char buf[], int level, char line[]) {
    int len = strlen(buf);
    for(;level < len; ++level) {
        if(buf[level] == '\r' && buf[level + 1] == '\n')
            return level + 2;
        else
            *line++ = buf[level];
    }
    return -1;
}

WebSocket_CommunicationType getType(char buffer[]) {
    char type;
    type = buffer[0] & 0x0F;
    if ((buffer[0] & 0x80) == 0x80) {
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

int getPayLoadLen(Client *client) {
    return client->recv_buf[1] & 0x7F;
}

int isMask(Client *client) {
    return ((client->recv_buf[1] & 0x80) == 0x80);
}

int getDataLen(Client *client, int payLoadLen) {
    int dataLen = 0;
    if(payLoadLen == 126) {
        dataLen = client->recv_buf[2];
        dataLen = (dataLen << 8) + client->recv_buf[3];
    } else if(payLoadLen == 127) {
        if(client->recv_buf[2] == 0 && client->recv_buf[3] == 0 && client->recv_buf[4] == 0 && client->recv_buf[5] == 0) {
            dataLen = client->recv_buf[6];
            dataLen = (dataLen << 8) + client->recv_buf[7];
            dataLen = (dataLen << 8) + client->recv_buf[8];
            dataLen = (dataLen << 8) + client->recv_buf[9];
        }
    } else {
        dataLen = client->recv_buf[1] & 0x7F;
    }

    return dataLen;
}

int getPackageLen(int ismask, int payLoadLen, int dataLen) {
    int packageLen = 0;

    if(ismask) {
        packageLen += MASK_LEN;
    }
    if(payLoadLen == 126) {
        packageLen += 4;
    } else if(payLoadLen == 127) {
        packageLen += 10;
    } else {
        packageLen += 2;
    }

    packageLen += dataLen;

    return packageLen;
}

int dePackage(char package[], int ismask, int payLoadLen, int dataLen, char message[]) {
    char mask_key[MASK_LEN];
    int i, start;
    char temp1, temp2;

    memset(mask_key, 0, MASK_LEN);
    if(payLoadLen == 126) {
        if(ismask) {
            mask_key[0] = package[4];
            mask_key[1] = package[5];
            mask_key[2] = package[6];
            mask_key[3] = package[7];
            start = 8;
        } else
            start = 4;
    } else if(payLoadLen == 127) {
        if(ismask) {
            mask_key[0] = package[10];
            mask_key[1] = package[11];
            mask_key[2] = package[12];
            mask_key[3] = package[13];
            start = 14;
        } else
            start = 10;
    } else {
        if(ismask) {
            mask_key[0] = package[2];
            mask_key[1] = package[3];
            mask_key[2] = package[4];
            mask_key[3] = package[5];
            start = 6;
        } else
            start = 2;
    }

    if(ismask) {
        for(i = 0; i < dataLen; ++i) {
            temp1 = package[i + start];
            temp2 = mask_key[i % MASK_LEN];
            message[i] = (char)((temp1 & (~temp2)) | ((~temp1) & temp2));
        }

        return 1;
    } else {
        memcpy(message, package + start, dataLen);
        message[dataLen] = '\0';

        return 1;
    }

    return 0;
}

int enpackage(char data[], int dataLen, WebSocket_CommunicationType type, bool isMask, char package[], unsigned int packageMaxLen) {
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
        package[dataLen] = '\0';
//        len += dataLen;
    }
    len += dataLen;
//    *package = *send;

    return len;
}


/**
 *
 * @param client
 * @return -1  客户端出错/关闭客户端
 *         0   读/写失败但客户端依然连接或数据不够长
 *         1   成功
 *         len 发送包的长度
 */
int checkState(Client *client, char data[]) {

    client_state state = client->getState();
    if(state == CS_TCP_CONNECTED) {
        int ret;
        char requestKey[LINE_MAX];
        char responseHead[LINE_MAX];

        if(client->getRecvNum() != 0) {
            memset(requestKey, 0, LINE_MAX);
            ret = getRequestKey(client, requestKey);
            if(ret == -1) {
                printf("client (%s:%d) shake hand failed\n", inet_ntoa(client->getClientAddress().sin_addr), client->getClientAddress().sin_port);
                return -1;
            } else if(ret == 0) {
                printf("客户端 (%s:%d) 头长度不足\n", inet_ntoa(client->getClientAddress().sin_addr), client->getClientAddress().sin_port);
                return 0;
            } else {
                printf("requestKey = %s\n", requestKey);
                memset(responseHead, 0, LINE_MAX);
                ret = getResponseHead(requestKey, responseHead);
                if(-1 == ret) {
                    printf("client (%s:%d) get response head error\n", inet_ntoa(client->getClientAddress().sin_addr), client->getClientAddress().sin_port);
                    return -1;
                }
                printf("responseHead\n%s", responseHead);
                client->lockSend();
                memcpy(client->send_buf, responseHead, strlen(responseHead));
                client->setSendNum(strlen(responseHead));
                client->unlockSend();
                return 1;
            }
        }
    } else if(state == CS_WEBSOCKET_CONNECTED) {
        if(client->getRecvNum() > 1) {
            client->lockRecv();
            WebSocket_CommunicationType type = getType(client->recv_buf);
            client->unlockRecv();
            if(type == WCT_TXTDATA || type == WCT_BINDATA || type == WCT_PING) {
                int packageLen = 0, dataLen = 0, ismask = 0, payloadLen = 0;
                char message[LINE_MAX], package[LINE_MAX];

                if(client->getRecvNum() >= 2) {
                    client->lockRecv();
                    ismask = isMask(client);
                    payloadLen = getPayLoadLen(client);
                    client->unlockRecv();
                } else {
                    printf("客户端 (%s:%d) 数据不够长, 不足以获取判断掩码和数据长度的标记\n", inet_ntoa(client->getClientAddress().sin_addr), client->getClientAddress().sin_port);
                    return 0;
                }

                if(client->getRecvNum() >= packageLen) {
                    client->lockRecv();
                    dataLen = getDataLen(client, payloadLen);
                    client->unlockRecv();
                    packageLen = getPackageLen(ismask, payloadLen, dataLen);
                } else {
                    printf("客户端 (%s:%d) 数据不够长, 不足以获取数据长度\n", inet_ntoa(client->getClientAddress().sin_addr), client->getClientAddress().sin_port);
                    return 0;
                }

                if(client->getRecvNum() >= packageLen) {
                    memset(message, 0, LINE_MAX);
                    memset(package, 0, LINE_MAX);
                    client->lockRecv();
                    memcpy(package, client->recv_buf, packageLen);
                    client->unlockRecv();
                    int ret = dePackage(package, ismask, payloadLen, dataLen, message);
                    int len;
                    if(ret == 1) {

                        char dataPackage[LINE_MAX];
                        memset(dataPackage, 0, LINE_MAX);
                        printf("client(%s:%d) said: %s", inet_ntoa(client->getClientAddress().sin_addr), client->getClientAddress().sin_port, message);
                        sprintf(dataPackage, "client(%s:%d) said: %s",
                                inet_ntoa(client->getClientAddress().sin_addr), client->getClientAddress().sin_port, message);

                        client->lockRecv();
                        char temp[LINE_MAX];
                        memcpy(temp, client->recv_buf + packageLen, client->getRecvNum() - packageLen);
                        memcpy(client->recv_buf, temp, client->getRecvNum() - packageLen);
                        memset(client->recv_buf + client->getRecvNum() - packageLen, 0, packageLen);
                        client->setRecvNum(client->getRecvNum() - packageLen);
                        client->unlockRecv();

                        memset(data, 0, LINE_MAX);
                        if(type != WCT_PING) {
                            len = enpackage(dataPackage, strlen(dataPackage), type, false, data, LINE_MAX);
                            if(len == -1) {
                                printf("%d不够长\n", LINE_MAX);
                                return -1;
                            }
                            return len;
                        } else {
                            len = enpackage(message, dataLen, WCT_PONG, true, data, LINE_MAX);
                            client->lockSend();
                            memcpy(client->send_buf, data, len);
                            client->setSendNum(len);
                            client->unlockSend();
                            return 1;
                        }
                    } else {
                        printf("数据出现问题\n");
                        return 0;
                    }
                } else {
                    printf("客户端 (%s:%d) 数据不够长, 不足以获取数据\n", inet_ntoa(client->getClientAddress().sin_addr), client->getClientAddress().sin_port);
                    return 0;
                }
            } else if(type == WCT_DISCONN) {
                client->setState(CS_CLOSE);
            }
            return 0;
        }

    } else if(state == CS_CLOSE) {
        return -1;
    }

//    return -1;
}

/**
 *
 * @param client
 * @param requestKey
 * @return 1  成功
 *         0  数据不全
 *         -1 失败
 */
int getRequestKey(Client *client, char requestKey[]) {
    int headLen = 0, headEnd = 0;
    char head[LINE_MAX], temp[LINE_MAX], line[LINE_MAX], *end;
    int i, level = 0;
    if(client->getRecvNum() < 3) {
        return 0;
    }
    if(client->recv_buf[0] == 'G' && client->recv_buf[1] == 'E' && client->recv_buf[2] == 'T') {
        if(client->getRecvNum() < 7) {
            return 0;
        }

        for(i = 0; i + 3 < client->getRecvNum(); ++i) {
            if(client->recv_buf[i] == '\r' && client->recv_buf[i + 1] == '\n' && client->recv_buf[i + 2] == '\r' && client->recv_buf[i + 3] == '\n')
                headEnd = i + 3;
        }

        if((end = strstr(client->recv_buf, "\r\n\r\n")) != NULL) {
            memset(head, 0, LINE_MAX);
            memcpy(head, client->recv_buf, client->getRecvNum() - strlen(end + 4));
        } else {
            if(client->getRecvNum() > LINE_MAX) {
                printf("head length error\n");
                return -1;
            }
            return 0;
        }
    } else {
        printf("get head error\n");
        return -1;
    }

    headLen = strlen(head);
    if(headLen != 0) {
        memset(temp, 0, LINE_MAX);
        client->lockRecv();
        memcpy(temp, client->recv_buf + headLen + 1, client->getRecvNum() - headLen);
        memcpy(client->recv_buf, temp, client->getRecvNum() - headLen);
        memset(client->recv_buf + headLen + 1, 0, headLen);
        client->setRecvNum(client->getRecvNum() - headLen);
        client->unlockRecv();
    } else {
        printf("client(%s:%d) head is empty\n", inet_ntoa(client->getClientAddress().sin_addr), client->getClientAddress().sin_port);
        return -1;
    }

    do {
        memset(line, 0, 1024);
        level = readLine(head, level, line);
        if(strstr(line, "Sec-WebSocket-Key: ") != NULL) {
            memcpy(requestKey, line + 19, strlen(line + 19));
            return 1;
        }
    } while(level != -1);

    printf("client(%s:%d) Sec-WebSocket-Key not found\n", inet_ntoa(client->getClientAddress().sin_addr),
           client->getClientAddress().sin_port);
    return -1;
}

/**
 *
 * @param requestKey
 * @param responseHead
 * @return 1  成功
 *         -1 失败
 */
int getResponseHead(char requestKey[], char responseHead[]) {
    char sha1Data[LINE_MAX];
    char responseKey[LINE_MAX];
    const char *GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    memset(responseKey, 0, LINE_MAX);
    if(strlen(requestKey) <= 0) {
        printf("requestKey error\n");
        return -1;
    }
    printf("requestKey = %s\n", requestKey);
    printf("strlen(requestKey) = %lu", strlen(requestKey));
    strcat(requestKey, GUID);
    SHA1((unsigned char *)requestKey, strlen(requestKey), (unsigned char *)sha1Data);
    base64_encode(sha1Data, strlen(sha1Data), responseKey);

    sprintf(responseHead, "HTTP/1.1 101 Switching Protocols\r\n"
                             "Upgrade: websocket\r\n"
                             "Connection: Upgrade\r\n"
                             "Sec-WebSocket-Accept: %s\r\n\r\n", responseKey);

    return 1;
}

void delayms(int ms) {
    struct timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = ms % 1000 * 1000;
    select(0, NULL, NULL, NULL, &tv);
}
