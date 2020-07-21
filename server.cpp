#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>
#include <cstring>
#include <errno.h>

#define MAX_INPUT 1024
using namespace std;

int main() {
    struct sockaddr_in sockaddr;
    int sockfd, listenfd, connfd;
    char buf[MAX_INPUT];
    int n, port = 10051;
    int on = 1;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) {
        printf("创建套接字失败！");
        return -1;
    }

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)); //允许地址的立即重用

    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);   //接受任意IP地址的客户链接
    sockaddr.sin_port = htonl(port);    //服务端指定的端号


    if (bind(sockfd, (struct sockaddr *) &sockaddr, sizeof(sockaddr)) == -1) {
        printf("fail to bind socket : %d\n", errno);
        return -1;
    }
}
