#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

// TCP client
int main() {

    // 1.创建套接字
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd == -1) {
        perror("socket");
        exit(-1);
    }

    // 2.连接服务器端
    struct sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    inet_pton(AF_INET, "192.168.146.131", &serveraddr.sin_addr.s_addr);
    serveraddr.sin_port = htons(10000);
    int ret = connect(fd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));

    if (ret == -1) {
        perror("connect");
        exit(-1);
    }

    char clientIP[16];
    u_int16_t clientPort;

    inet_ntop(AF_INET, &serveraddr.sin_addr.s_addr, clientIP, sizeof(clientIP));
    clientPort = ntohs(serveraddr.sin_port);

    printf("A TCP connection is established: ip - %s, port - %d\n", clientIP, clientPort);

    while (1);

    // 关闭连接
    close(fd);

    return 0;
}
