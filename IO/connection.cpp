#include <arpa/inet.h>
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/epoll.h>

#define BUFFER_SIZE 1023

int setNoBlocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// 超时连接函数，参数分别是服务器ip地址，端口号和超时时间。函数成功返回已经处于连接状态的socket，失败则返回-1
int unblock_connection(const char* ip, int port, int time) {
    int ret = 0;
    sockaddr_in address;
    bzero(&address, sizeof(address));
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    int fdopt = setNoBlocking(sockfd);
    ret = connect(sockfd, (sockaddr*)&address, sizeof(address));
    if (ret == 0) {
        // 如果连接成功，则恢复sockfd属性，并立即返回
        fcntl(sockfd, F_SETFL, fdopt);
        return sockfd;
    }
    else if (errno != EINPROGRESS) {
        // 如果连接没有立即建立，那么只有当errno是EINPROGRESS时才表示连接还在进行
        return -1;
    }
    fd_set readfds;
    fd_set writefds;
    timeval timeout;

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);

    FD_SET(sockfd, &writefds);
    timeout.tv_sec = time;
    timeout.tv_usec = 0;

    ret = select(sockfd + 1, NULL, &writefds, NULL, &timeout);
    if (ret <= 0) {
        // select超时或者出错
        close(sockfd);
        return -1;
    }

    if (!FD_ISSET(sockfd, &writefds)) {
        close(sockfd);
        return -1;
    }

    int error = 0;
    socklen_t len = sizeof(error);

    // 调用getsockopt来获取并清除sockfd上的错误
    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
        close(sockfd);
        return -1;
    }

    // 错误号不为0表示连接错误
    if (error != 0) {
        close(sockfd);
        return -1;
    }
    // 连接成功
    fcntl(sockfd, F_SETFL, fdopt);
    return sockfd;
}

int main(int argc, char const *argv[])
{
    if (argc <= 2) {
        return -1;
    }   
    const char* ip = argv[1];
    int port = atoi(argv[2]);

    int sockfd = unblock_connection(ip, port, 10);
    if (sockfd < 0) {
        return 1;
    }
    close(sockfd);
    return 0;
}