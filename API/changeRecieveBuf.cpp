#include <arpa/inet.h>
#include <cstdlib>
#include <netinet/in.h>
#include <strings.h>
#include <sys/socket.h>
int main(int argc, char* argv[]) {
    if (argc <= 2) {
        return -1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);

    sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &address.sin_addr);

    int sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
    int recvbuf = atoi(argv[3]);
    int len = sizeof(recvbuf);

    // 先设置TCP接受缓冲区大小，然后立即读
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &recvbuf, len);
    getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF , &recvbuf, (socklen_t*)&len);

    return 0;
}