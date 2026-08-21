#include <arpa/inet.h>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <strings.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/select.h>

int main(int argc, char const *argv[])
{
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
    assert(sockfd >= 0);

    int ret = bind(sockfd, (sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(sockfd, 128);
    assert(ret != -1);

    sockaddr_in client;
    socklen_t len = sizeof(client);
    int connfd = accept(sockfd, (sockaddr*)&client, &len);
    if (connfd < 0) {
        return -1;
    }

    char buf[1024];
    fd_set read_fds;
    fd_set exception_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&exception_fds);

    while(1) {
        memset(buf, '\0', sizeof(buf));
        // 每一次调用select前都要重新在read_fds和exception_fds中设置文件描述符connfd，因为事情发生了之后，文件描述符集合将被内核修改
        FD_SET (connfd, &read_fds);
        FD_SET (connfd, &exception_fds);
        ret = select(connfd + 1, &read_fds, NULL, &exception_fds, NULL);
        if (ret < 0) {
            std::cout << "select failed" << std::endl;
            break;
        }
        // 对于可读事件，采用普通的recv函数读取数据
        if (FD_ISSET(connfd, &read_fds)) {
            ret = recv(connfd, buf, sizeof(buf), 0);
            if (ret <= 0) {
                break;
            }
        }
        else if (FD_ISSET(connfd, &exception_fds)) {
            ret = recv(connfd, buf, sizeof(buf), MSG_OOB);
            if (ret < 0) {
                break;
            }
        }
    }
    close (connfd);
    close (sockfd);
    return 0;
}
