#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <stdlib.h>
#include <errno.h>

int main() {
    int sockfd;
    struct sockaddr_in s_addr;
    int port = 6666;

    if(-1 == (sockfd = socket(AF_INET, SOCK_DGRAM, 0))) {
        printf("create socket failed, error: %s, errno: %d\n", strerror(errno), errno);
        return -1;
    }

    memset(&s_addr, 0, sizeof(s_addr));
    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = inet_addr("192.168.0.110");
    s_addr.sin_port = htons(port);

    int send_num, recv_num;
    char send_buf[20] = "hi, i'm client", recv_buf[20];
    size_t size = sizeof(struct sockaddr_in);

    send_num = sendto(sockfd, send_buf, strlen(send_buf), 0, (struct sockaddr *)&s_addr, (socklen_t)size);
    if(-1 == send_num) {
        printf("send failed, error: %s, errno: %d\n", strerror(errno), errno);
        return -1;
    }

    recv_num = recvfrom(sockfd, recv_buf, sizeof(recv_buf), 0, (struct sockaddr *)&s_addr, (socklen_t *)&size);
    if(-1 == recv_num) {
        printf("receive failed, error: %s, errno: %d\n", strerror(errno), errno);
        return -1;
    }
    recv_buf[recv_num] = '\0';
    printf("receive from server: %s", recv_buf);

    close(sockfd);

    return 0;
}
