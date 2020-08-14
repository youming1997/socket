//
// Created by 朱宏宽 on 2020/8/11.
//

#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <errno.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <event2/event.h>

#include "../include/websocket_common.h"
#include "../include/base64.h"
#include "../include/sha1.h"

#define LINE_MAX 1024
#define SHA_DIGEST_LENGTH 128
#define GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

/*----------------------数据帧格式粘出来----------------------------
0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-------+-+-------------+-------------------------------+
|F|R|R|R| opcode|M| Payload len |    Extended payload length    |
|I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
|N|V|V|V|       |S|             |   (if payload len==126/127)   |
| |1|2|3|       |K|             |                               |
+-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
|     Extended payload length continued, if payload len == 127  |
+ - - - - - - - - - - - - - - - +-------------------------------+
|                               |Masking-key, if MASK set to 1  |
+-------------------------------+-------------------------------+
| Masking-key (continued)       |          Payload Data         |
+-------------------------------- - - - - - - - - - - - - - - - +
:                     Payload Data continued ...                :
+ - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
|                     Payload Data continued ...                |
+---------------------------------------------------------------+
-----------------------------------------------------------------*/
typedef struct frame_header {
    char fin;
    char opcode;
    char mask;
    unsigned long long payload_length;
    char masking_key[4];
}frame_head;

int socket_bind_listen(int port, int listen_num) {
    int listenfd;
    if(-1 == (listenfd = socket(AF_INET, SOCK_STREAM, 0))) {
        perror("socket error");
        exit(1);
    }

    struct sockaddr_in s_addr;
    socklen_t socklen = sizeof(struct sockaddr_in);
    memset(&s_addr, 0, socklen);
    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    s_addr.sin_port = htons(port);
    if(-1 == (bind(listenfd, (struct sockaddr *)&s_addr, socklen))) {
        perror("bind error");
        exit(1);
    }
    if(-1 == (listen(listenfd, listen_num))) {
        perror("socket error");
        exit(1);
    }

    printf("监听 %d 端\n", port);
    return listenfd;
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

int shakeHand(int clntfd) {
    int level = 0;                                  //每行的起始位置
    char buf[LINE_MAX];                             //数据
    char line[LINE_MAX];                            //每行数据
    char *sec_accept;                            //Sec-WebSocket-Accept
    char *sha1_data; //
    char head[LINE_MAX] = {0};

    if((recv(clntfd, buf, LINE_MAX, 0)) < 0) {
        perror("recv error");
    }
    printf("request: \n");
    printf("%s\n", buf);

    do {
        memset(line, 0, LINE_MAX);
        level = readLine(buf, level, line);
        printf("line: %s\n", line);

        if(strstr(line, "Sec-WebSocket-Key: ") != NULL) {
            strcat(line, GUID);

            sha1_data = sha1_hash(line);

            sec_accept = base64_encode((const char *)(sha1_data), strlen((const char *)sha1_data));

            sprintf(head, "HTTP/1.1 101 Switching Protocols\r\n" \
                          "Upgrade: websocket\r\n" \
                          "Server: Microsoft-HTTPAPI/2.0\r\n" \
                          "Connection: Upgrade\r\n" \
                          "Sec-WebSocket-Accept: %s\r\n", sec_accept);

            printf("response\n");
            printf("%s", head);
            if(send(clntfd, head, strlen(head), 0) < 0) {
                perror("send error");
            }
            break;
        }
    } while (buf[level] != '\r' || buf[level + 1] != '\n');

    return 0;
}

void inverted_string(char *str,int len)
{
    int i; char temp;
    for (i=0;i<len/2;++i)
    {
        temp = *(str+i);
        *(str+i) = *(str+len-i-1);
        *(str+len-i-1) = temp;
    }
}

int recv_frame_head(int fd,frame_head* head)
{
    char one_char;
    /*read fin and op code*/
    if (read(fd,&one_char,1)<=0)
    {
        perror("read fin");
        return -1;
    }
    head->fin = (one_char & 0x80) == 0x80;
    head->opcode = one_char & 0x0F;
    if (read(fd,&one_char,1)<=0)
    {
        perror("read mask");
        return -1;
    }
    head->mask = (one_char & 0x80) == 0X80;

    /*get payload length*/
    head->payload_length = one_char & 0x7F;

    if (head->payload_length == 126)
    {
        char extern_len[2];
        if (read(fd,extern_len,2)<=0)
        {
            perror("read extern_len");
            return -1;
        }
        head->payload_length = (extern_len[0]&0xFF) << 8 | (extern_len[1]&0xFF);
    }
    else if (head->payload_length == 127)
    {
        char extern_len[8];
        if (read(fd,extern_len,8)<=0)
        {
            perror("read extern_len");
            return -1;
        }
        inverted_string(extern_len,8);
        memcpy(&(head->payload_length),extern_len,8);
    }

    /*read masking-key*/
    if (read(fd,head->masking_key,4)<=0)
    {
        perror("read masking-key");
        return -1;
    }

    return 0;
}

void umask(char *data,int len,char *mask)
{
    int i;
    for (i=0;i<len;++i)
        *(data+i) ^= *(mask+(i%4));
}

int send_frame_head(int fd,frame_head* head)
{
    char *response_head;
    int head_length = 0;
    if(head->payload_length<126)
    {
        response_head = (char*)malloc(2);
        response_head[0] = 0x81;
        response_head[1] = head->payload_length;
        head_length = 2;
    }
    else if (head->payload_length<0xFFFF)
    {
        response_head = (char*)malloc(4);
        response_head[0] = 0x81;
        response_head[1] = 126;
        response_head[2] = (head->payload_length >> 8 & 0xFF);
        response_head[3] = (head->payload_length & 0xFF);
        head_length = 4;
    }
    else
    {
        response_head = (char*)malloc(12);
        response_head[0] = 0x81;
        response_head[1] = 127;
        memcpy(response_head+2, (const void *)head->payload_length, sizeof(unsigned long long));
        inverted_string(response_head+2,sizeof(unsigned long long));
        head_length = 12;
    }

    if(write(fd,response_head,head_length)<=0)
    {
        perror("write head");
        return -1;
    }

    free(response_head);
    return 0;
}

int main()
{
    int servfd = socket_bind_listen(4444,20);


    struct sockaddr_in client_addr;
    socklen_t addr_length = sizeof(client_addr);
    int connfd = accept(servfd,(struct sockaddr*)&client_addr, &addr_length);

    shakeHand(connfd);

    while (1)
    {
        frame_head head;
        int rul = recv_frame_head(connfd,&head);
        if (rul < 0)
            break;
//        printf("fin=%d\nopcode=0x%X\nmask=%d\npayload_len=%llu\n",head.fin,head.opcode,head.mask,head.payload_length);

        //echo head
        send_frame_head(connfd,&head);
        //read payload data
        char payload_data[1024] = {0};
        int size = 0;
        do {
            int rul;
            rul = read(connfd,payload_data,1024);
            if (rul<=0)
                break;
            size+=rul;

            umask(payload_data,size,head.masking_key);
            printf("recive:%s",payload_data);

            //echo data
            if (write(connfd,payload_data,rul)<=0)
                break;
        }while(size<head.payload_length);
        printf("\n-----------\n");

    }

    close(connfd);
    close(servfd);
}
