#include <arpa/inet.h>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>

#define BUFFER_SIZE 512

int main(int argc, char* argv[]) {
    if (argc <= 2) {
        return -1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);

    sockaddr_in server_address;
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server_address.sin_addr);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);

    int sendbuf = atoi(argv[3]);
    int len = sizeof(sendbuf);
    // 先设置TCP发送缓冲区的大小，然后立即读取，这个sendbuf传递的就是我们想要修改的内存大小
    setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &sendbuf, sizeof(sendbuf));
    getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &sendbuf, (socklen_t*)&len);

    if (connect(sockfd, (sockaddr*)& server_address, sizeof(server_address)) != -1) {
        char buffer[BUFFER_SIZE];
        memset(buffer, 'a', sizeof(buffer));
        send(sockfd, buffer, BUFFER_SIZE, 0);
    }
    close(sockfd);
    return 0;
}