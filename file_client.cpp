#include<netinet/in.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <arpa/inet.h>
#include <zconf.h>

#define SERVER_PORT 6666
#define buf_SIZE 1024
#define FILE_NAME_MAX_SIZE 512

int main()
{
    struct sockaddr_in s_addr;
    int sockfd;

    if(-1 == (sockfd = socket(AF_INET, SOCK_STREAM, 0)))
    {
        perror("Create Socket Failed:");
        exit(1);
    }

    memset(&s_addr, 0, sizeof(s_addr));
    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = inet_addr("192.168.0.109");
    s_addr.sin_port = htons(SERVER_PORT);
    socklen_t len = sizeof(s_addr);

    if(connect(sockfd, (struct sockaddr*)&s_addr, len) < 0)
    {
        perror("Can Not Connect To Server IP:");
        exit(0);
    }

    char file_name[FILE_NAME_MAX_SIZE+1];
    int flag = 1;
    while(flag) {
        bzero(file_name, FILE_NAME_MAX_SIZE+1);
        printf("Please Input File Name On Server: ");
        scanf("%s", file_name);
        if(strlen(file_name) != 0) {
            flag = 0;
            continue;
        }
        printf("filename is empty, please rewrite.\n");
    }

    char buf[buf_SIZE];
    bzero(buf, buf_SIZE);
    strncpy(buf, file_name, strlen(file_name)>buf_SIZE?buf_SIZE:strlen(file_name));

    ssize_t l;
    if(0 == (l = send(sockfd, buf, buf_SIZE, 0))) {
        printf("client has been shutdown.\n");
        close(sockfd);
        exit(1);
    }
    if(l == -1)
    {
        perror("Send File Name Failed:");
        exit(1);
    }

    FILE *fp;

    bzero(buf, buf_SIZE);
    int length = 0;
    fp = fopen(file_name, "wb");
    if(NULL == fp)
    {
        printf("File: %s Can Not Open To Write\n", file_name);
        close(sockfd);
        exit(1);
    }
    while(1)
    {
        length = recv(sockfd, buf, buf_SIZE, 0);
        if(length == 0) {
            break;
        }
        if(fwrite(buf, sizeof(char), length, fp) < 0)
        {
            printf("File: %s Write Failed\n", file_name);
            remove(file_name);
            break;
        }
    }
    if(NULL != fp) {
        printf("Receive File: %s From Server IP Successful!\n", file_name);
        fclose(fp);
    } else {
        printf("File: %s Create Failed.\n", file_name);
    }
    close(sockfd);
    return 0;
}
