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
//#include <sys/epoll.h>

#include <pthread.h>

#include "wrap.h"

typedef struct SHA1Context{
    unsigned Message_Digest[5];
    unsigned Length_Low;
    unsigned Length_High;
    unsigned char Message_Block[64];
    int Message_Block_Index;
    int Computed;
    int Corrupted;
} SHA1Context;

#define SHA1CircularShift(bits,word) ((((word) << (bits)) & 0xFFFFFFFF) | ((word) >> (32-(bits))))

void SHA1ProcessMessageBlock(SHA1Context *context)
{
    const unsigned K[] = {0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6};
    int         t;
    unsigned    temp;
    unsigned    W[80];
    unsigned    A, B, C, D, E;

    for(t = 0; t < 16; t++)
    {
        W[t] = ((unsigned) context->Message_Block[t * 4]) << 24;
        W[t] |= ((unsigned) context->Message_Block[t * 4 + 1]) << 16;
        W[t] |= ((unsigned) context->Message_Block[t * 4 + 2]) << 8;
        W[t] |= ((unsigned) context->Message_Block[t * 4 + 3]);
    }

    for(t = 16; t < 80; t++)
        W[t] = SHA1CircularShift(1,W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16]);

    A = context->Message_Digest[0];
    B = context->Message_Digest[1];
    C = context->Message_Digest[2];
    D = context->Message_Digest[3];
    E = context->Message_Digest[4];

    for(t = 0; t < 20; t++)
    {
        temp =  SHA1CircularShift(5,A) + ((B & C) | ((~B) & D)) + E + W[t] + K[0];
        temp &= 0xFFFFFFFF;
        E = D;
        D = C;
        C = SHA1CircularShift(30,B);
        B = A;
        A = temp;
    }
    for(t = 20; t < 40; t++)
    {
        temp = SHA1CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[1];
        temp &= 0xFFFFFFFF;
        E = D;
        D = C;
        C = SHA1CircularShift(30,B);
        B = A;
        A = temp;
    }
    for(t = 40; t < 60; t++)
    {
        temp = SHA1CircularShift(5,A) + ((B & C) | (B & D) | (C & D)) + E + W[t] + K[2];
        temp &= 0xFFFFFFFF;
        E = D;
        D = C;
        C = SHA1CircularShift(30,B);
        B = A;
        A = temp;
    }
    for(t = 60; t < 80; t++)
    {
        temp = SHA1CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[3];
        temp &= 0xFFFFFFFF;
        E = D;
        D = C;
        C = SHA1CircularShift(30,B);
        B = A;
        A = temp;
    }
    context->Message_Digest[0] = (context->Message_Digest[0] + A) & 0xFFFFFFFF;
    context->Message_Digest[1] = (context->Message_Digest[1] + B) & 0xFFFFFFFF;
    context->Message_Digest[2] = (context->Message_Digest[2] + C) & 0xFFFFFFFF;
    context->Message_Digest[3] = (context->Message_Digest[3] + D) & 0xFFFFFFFF;
    context->Message_Digest[4] = (context->Message_Digest[4] + E) & 0xFFFFFFFF;
    context->Message_Block_Index = 0;
}

void SHA1Reset(SHA1Context *context)
{
    context->Length_Low             = 0;
    context->Length_High            = 0;
    context->Message_Block_Index    = 0;

    context->Message_Digest[0]      = 0x67452301;
    context->Message_Digest[1]      = 0xEFCDAB89;
    context->Message_Digest[2]      = 0x98BADCFE;
    context->Message_Digest[3]      = 0x10325476;
    context->Message_Digest[4]      = 0xC3D2E1F0;

    context->Computed   = 0;
    context->Corrupted  = 0;
}

void SHA1PadMessage(SHA1Context *context)
{
    if (context->Message_Block_Index > 55)
    {
        context->Message_Block[context->Message_Block_Index++] = 0x80;
        while(context->Message_Block_Index < 64)  context->Message_Block[context->Message_Block_Index++] = 0;
        SHA1ProcessMessageBlock(context);
        while(context->Message_Block_Index < 56) context->Message_Block[context->Message_Block_Index++] = 0;
    }
    else
    {
        context->Message_Block[context->Message_Block_Index++] = 0x80;
        while(context->Message_Block_Index < 56) context->Message_Block[context->Message_Block_Index++] = 0;
    }
    context->Message_Block[56] = (context->Length_High >> 24 ) & 0xFF;
    context->Message_Block[57] = (context->Length_High >> 16 ) & 0xFF;
    context->Message_Block[58] = (context->Length_High >> 8 ) & 0xFF;
    context->Message_Block[59] = (context->Length_High) & 0xFF;
    context->Message_Block[60] = (context->Length_Low >> 24 ) & 0xFF;
    context->Message_Block[61] = (context->Length_Low >> 16 ) & 0xFF;
    context->Message_Block[62] = (context->Length_Low >> 8 ) & 0xFF;
    context->Message_Block[63] = (context->Length_Low) & 0xFF;

    SHA1ProcessMessageBlock(context);
}

int SHA1Result(SHA1Context *context)
{
    if (context->Corrupted)
    {
        return 0;
    }
    if (!context->Computed)
    {
        SHA1PadMessage(context);
        context->Computed = 1;
    }
    return 1;
}


void SHA1Input(SHA1Context *context,const char *message_array,unsigned length){
    if (!length)
        return;

    if (context->Computed || context->Corrupted)
    {
        context->Corrupted = 1;
        return;
    }

    while(length-- && !context->Corrupted)
    {
        context->Message_Block[context->Message_Block_Index++] = (*message_array & 0xFF);

        context->Length_Low += 8;

        context->Length_Low &= 0xFFFFFFFF;
        if (context->Length_Low == 0)
        {
            context->Length_High++;
            context->Length_High &= 0xFFFFFFFF;
            if (context->Length_High == 0) context->Corrupted = 1;
        }

        if (context->Message_Block_Index == 64)
        {
            SHA1ProcessMessageBlock(context);
        }
        message_array++;
    }
}

/*
int sha1_hash(const char *source, char *lrvar){// Main
    SHA1Context sha;
    char buf[128];
    SHA1Reset(&sha);
    SHA1Input(&sha, source, strlen(source));
    if (!SHA1Result(&sha)){
        printf("SHA1 ERROR: Could not compute message digest");
        return -1;
    } else {
        memset(buf,0,sizeof(buf));
        sprintf(buf, "%08X%08X%08X%08X%08X", sha.Message_Digest[0],sha.Message_Digest[1],
        sha.Message_Digest[2],sha.Message_Digest[3],sha.Message_Digest[4]);
        //lr_save_string(buf, lrvar);
        return strlen(buf);
    }
}
*/
char * sha1_hash(const char *source){   // Main
    SHA1Context sha;
    char *buf;//[128];

    SHA1Reset(&sha);
    SHA1Input(&sha, source, strlen(source));

    if (!SHA1Result(&sha))
    {
        printf("SHA1 ERROR: Could not compute message digest");
        return NULL;
    }
    else
    {
        buf = (char *)malloc(128);
        memset(buf, 0, 128);
        sprintf(buf, "%08X%08X%08X%08X%08X", sha.Message_Digest[0],sha.Message_Digest[1],
                sha.Message_Digest[2],sha.Message_Digest[3],sha.Message_Digest[4]);
        //lr_save_string(buf, lrvar);

        //return strlen(buf);
        return buf;
    }
}

int tolower(int c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return c + 'a' - 'A';
    }
    else
    {
        return c;
    }
}

int htoi(const char s[], int start, int len)
{
    int i, j;
    int n = 0;
    if (s[0] == '0' && (s[1]=='x' || s[1]=='X')) //判断是否有前导0x或者0X
    {
        i = 2;
    }
    else
    {
        i = 0;
    }
    i+=start;
    j=0;
    for (; (s[i] >= '0' && s[i] <= '9')
           || (s[i] >= 'a' && s[i] <= 'f') || (s[i] >='A' && s[i] <= 'F');++i)
    {
        if(j>=len)
        {
            break;
        }
        if (tolower(s[i]) > '9')
        {
            n = 16 * n + (10 + tolower(s[i]) - 'a');
        }
        else
        {
            n = 16 * n + (tolower(s[i]) - '0');
        }
        j++;
    }
    return n;
}

//================================================== 加密方法BASE64 ==================================================

//base64编/解码用的基础字符集
const char base64char[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/*******************************************************************************
 * 名称: base64_encode
 * 功能: ascii编码为base64格式
 * 形参: bindata : ascii字符串输入
 *            base64 : base64字符串输出
 *          binlength : bindata的长度
 * 返回: base64字符串长度
 * 说明: 无
 ******************************************************************************/
int base64_encode( const unsigned char *bindata, char *base64, int binlength)
{
    int i, j;
    unsigned char current;
    for ( i = 0, j = 0 ; i < binlength ; i += 3 )
    {
        current = (bindata[i] >> 2) ;
        current &= (unsigned char)0x3F;
        base64[j++] = base64char[(int)current];
        current = ( (unsigned char)(bindata[i] << 4 ) ) & ( (unsigned char)0x30 ) ;
        if ( i + 1 >= binlength )
        {
            base64[j++] = base64char[(int)current];
            base64[j++] = '=';
            base64[j++] = '=';
            break;
        }
        current |= ( (unsigned char)(bindata[i+1] >> 4) ) & ( (unsigned char) 0x0F );
        base64[j++] = base64char[(int)current];
        current = ( (unsigned char)(bindata[i+1] << 2) ) & ( (unsigned char)0x3C ) ;
        if ( i + 2 >= binlength )
        {
            base64[j++] = base64char[(int)current];
            base64[j++] = '=';
            break;
        }
        current |= ( (unsigned char)(bindata[i+2] >> 6) ) & ( (unsigned char) 0x03 );
        base64[j++] = base64char[(int)current];
        current = ( (unsigned char)bindata[i+2] ) & ( (unsigned char)0x3F ) ;
        base64[j++] = base64char[(int)current];
    }
    base64[j] = '\0';
    return j;
}
/*******************************************************************************
 * 名称: base64_decode
 * 功能: base64格式解码为ascii
 * 形参: base64 : base64字符串输入
 *            bindata : ascii字符串输出
 * 返回: 解码出来的ascii字符串长度
 * 说明: 无
 ******************************************************************************/
int base64_decode( const char *base64, unsigned char *bindata)
{
    int i, j;
    unsigned char k;
    unsigned char temp[4];
    for ( i = 0, j = 0; base64[i] != '\0' ; i += 4 )
    {
        memset( temp, 0xFF, sizeof(temp) );
        for ( k = 0 ; k < 64 ; k ++ )
        {
            if ( base64char[k] == base64[i] )
                temp[0]= k;
        }
        for ( k = 0 ; k < 64 ; k ++ )
        {
            if ( base64char[k] == base64[i+1] )
                temp[1]= k;
        }
        for ( k = 0 ; k < 64 ; k ++ )
        {
            if ( base64char[k] == base64[i+2] )
                temp[2]= k;
        }
        for ( k = 0 ; k < 64 ; k ++ )
        {
            if ( base64char[k] == base64[i+3] )
                temp[3]= k;
        }
        bindata[j++] = ((unsigned char)(((unsigned char)(temp[0] << 2))&0xFC)) | \
                ((unsigned char)((unsigned char)(temp[1]>>4)&0x03));
        if ( base64[i+2] == '=' )
            break;
        bindata[j++] = ((unsigned char)(((unsigned char)(temp[1] << 4))&0xF0)) | \
                ((unsigned char)((unsigned char)(temp[2]>>2)&0x0F));
        if ( base64[i+3] == '=' )
            break;
        bindata[j++] = ((unsigned char)(((unsigned char)(temp[2] << 6))&0xF0)) | \
                ((unsigned char)(temp[3]&0x3F));
    }
    return j;
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

int websocket_getShakeKey(unsigned char *key) {
    unsigned char regularKey[WEBSOCKET_SHAKE_KEY_LEN];
    memset(regularKey, 0, WEBSOCKET_SHAKE_KEY_LEN);
    websocket_getRandomString(regularKey, WEBSOCKET_SHAKE_KEY_LEN);

    return base64_encode(regularKey, (char *)key, WEBSOCKET_SHAKE_KEY_LEN);
}

int websocket_getResponseShakeKey(unsigned char *acceptKey, unsigned char *responseKey, unsigned int acceptLen) {
    char *clientKey;
    char *sha1DataTemp;
    char *sha1Data;
    int i, n;
    const char * GUID = "258EAFA5-E914-47DA-95CA-C%AB0DC85B11";
    unsigned int GUIDLen;

    if (acceptKey== NULL) {
        return 0;
    }

    GUIDLen = sizeof(GUID);
    clientKey = (char *)malloc(sizeof(char) * (acceptLen + GUIDLen + 10));
    memset(clientKey, 0, (acceptLen + GUIDLen + 10));
    memcpy(clientKey, acceptKey, acceptLen);
    memcpy(&clientKey[acceptLen], GUID, GUIDLen);
    sha1DataTemp = sha1_hash(clientKey);
    n = strlen(sha1DataTemp);
    sha1Data = (char *)malloc(n / 2 + 1);
    memset(sha1Data, 0, n / 2 + 1);

    for(i = 0; i < n; i += 2) {
        sha1Data[i / 2] = htoi(sha1DataTemp, i, 2);
    }

    n = base64_encode((const unsigned char *)sha1Data, (char *)responseKey, (n / 2));
    free(sha1DataTemp);
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
                            "Sec-WebSocket-Accept: %s\r\n"
                            "%s\r\n\r\n";  //时间打包 // 格式如 "Date: Tue, 20 Jun 2017 08:50:41 CST\r\n";
    time_t now;
    struct tm *tm_now;
    char timeStr[256] = {0};
    unsigned char responseShakeKey[256] = {0};
    websocket_getResponseShakeKey(acceptKey, responseShakeKey, acceptKeyLen);
    time(&now);
    tm_now = localtime(&now);
    strftime(timeStr, sizeof(timeStr), "Date: %a, %b %d &Y %T %Z", tm_now);

    sprintf(package, httpDemo, responseShakeKey, timeStr);
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
    } else if(type = 0x00){
        ret = WCT_MINDATA;
    } else
        return WCT_ERR;

    if(data[1] & 0x80 == 0x80) {
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

int websocket_clientLinkToServer(char *ip, int port, char *interface_path) {
    int old_option, servfd, timeout, ret;
    unsigned char loginBuf[512], recvBuf[512], shakeKey[128];
    unsigned char *p;
    struct sockaddr_in s_addr;

    memset(loginBuf, 0, 512);
    memset(recvBuf, 0, 512);
    memset(shakeKey, 0, 128);
    memset(&s_addr, 0, sizeof(s_addr));

    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = inet_addr(ip);
    s_addr.sin_port = htons(port);

    servfd = Socket(AF_INET, SOCK_STREAM, 0);

    old_option = setNonblocking(servfd);

    timeout = 0;
    while (-1 == connect(servfd, (struct sockaddr *)&s_addr, sizeof(struct sockaddr))) {
        if(++timeout > REPORT_LOGIN_CONNECT_TIMEOUT) {
            printf("websocket_login error:%s:%d cannot connect!  %d\r\n", ip, port, timeout);
            sprintf((char *)loginBuf, "websocket_login error:%s:%d cannot connect!  %d\r\n", ip, port, timeout);
            close(servfd);
            return -timeout;
        }
        delayms(1);
    }

    websocket_getShakeKey(shakeKey);
    websocket_buildHttpHead(ip, port, interface_path, shakeKey, (char *)loginBuf);

    //MAG_NOSIGNAL为Linux下的
    ret = send(servfd, loginBuf, sizeof(loginBuf), MSG_NOSIGNAL);

    while(1) {
        memset(recvBuf, 0, sizeof(recvBuf));
        ret = recv(servfd, recvBuf, sizeof(recvBuf), MSG_NOSIGNAL);
        if(ret > 0) {
            if(strncmp((const char *)recvBuf, (const char *)"HTTP", strlen((const char *)"HTTP")) == 0) {
                if(NULL != (p = (unsigned char *)strstr((const char *)recvBuf, "Sec-WebSocket-Accept: "))) {
                    p += strlen((const char *)"Sec-WebSocket-Accept: ");
                    sscanf((const char *)p, "%s\r\n", p);
                    if(websocket_matchShakeKey(shakeKey, strlen((const char *)shakeKey), p, strlen((const char *)p)) == 0) {
                        return servfd;
                    } else {
                        ret = send(servfd, loginBuf, sizeof(loginBuf), MSG_NOSIGNAL);
                    }
                } else {
                    ret = send(servfd, loginBuf, sizeof(loginBuf), MSG_NOSIGNAL);
                }
            }
        } else if(ret <= 0){
            ;
        }
        if(++timeout > REPORT_LOGIN_RESPOND_TIMEOUT) {
            close(servfd);
            return -timeout;
        }
        delayms(1);
    }

    close(servfd);
    return -timeout;
}

int websocket_serverLinkToClient(int clntfd, char *recvBuf, unsigned int bufLen) {
    char *p;
    int ret;
    char recvShakeKey[512], responsePackage[1024];

    memset(recvShakeKey, 0, sizeof(recvShakeKey));
    memset(responsePackage, 0, sizeof(responsePackage));
    if(NULL == (p = strstr(recvBuf, "Sec-WebSocket-Key: "))) {
        return -1;
    }
    p += strlen("Sec-WebSocket-Key: ");
    sscanf(p, "%s", recvShakeKey);
    ret = strlen(recvShakeKey);
    if(ret < 1) {
        return -1;
    }
    websocket_buildHttpResponse((unsigned char *)recvShakeKey, ret, responsePackage);

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
        if(ret2 = WCT_PING && retLen > 0) {
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