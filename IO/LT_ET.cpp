#include <arpa/inet.h>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <strings.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/epoll.h>

#define MAX_EVENT_NUMBER 1024
#define BUFFER_SIZE 10

int setNoBlocking (int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    // 这里返回原来的状态因为很多的时候我们需要原来的状态
    return old_option;
}

// 将文件描述符fd上的EPOLLN注册到epollfd指示的epoll内核事件表中，参数enable_et指定是否用ET模式
void addFd(int epollfd, int fd, bool enable_et) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN;
    if (enable_et) {
        event.events |= EPOLLET;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
}

// LT工作流程
void lt (epoll_event* events, int number, int epollfd, int listenfd) {
    char buf[BUFFER_SIZE];
    for (int i = 0; i < number; i++) {
        int sockfd = events[i].data.fd;
        if (sockfd == listenfd) {
            sockaddr_in clientAddress;
            socklen_t len = sizeof(clientAddress);
            int connfd = accept(listenfd, (sockaddr*)&clientAddress, &len);
            addFd(epollfd, connfd, false);
        }
        else if (events[i].events & EPOLLIN) {
            // 只要socket缓存中还有未读出的数据，这段代码就被触发
            memset(buf, '\0', sizeof(buf));
            int ret = recv(sockfd, buf, BUFFER_SIZE - 1, 0);
            if (ret <= 0) {
                close(sockfd);
                continue;
            }
        }
        else {
            /* code */
        }
    }
}

// et的工作流程
void et (epoll_event* events, int number, int epollfd, int listenfd) {
    char buf[BUFFER_SIZE];
    for (int i = 0; i < number; i++) {
        int sockfd = events[i].data.fd;
        if (sockfd == listenfd) {
            sockaddr_in clientAddress;
            socklen_t len = sizeof(clientAddress);
            int connfd = accept(listenfd, (sockaddr*)& clientAddress, &len);
            addFd(epollfd, connfd, true);
        }
        else if (events[i].events & EPOLLIN) {
            // 这段代码不会重复触发，所以我们要循环读取数据，以确保把socket读缓存里面的数据全部读完
            while(1) {
                memset(buf, '\0', BUFFER_SIZE);
                int ret = recv(sockfd, buf, BUFFER_SIZE - 1, 0);
                if (ret < 0) {
                    // 对于非阻塞IO，下面条件成立表示数据已经全部读取完毕，此后，epoll就可以被再一次触发sockfd上的EPOLLIN事件，以驱动下一次读操作
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        close(sockfd);
                        break;
                    }
                    else if (ret == 0) {
                        close(sockfd);
                    }
                    else {
                        std::cout << buf << std::endl;
                    }
                }
                else {
                    std::cout <<  "something else happened" << std::endl;
                }
            }
        }
    }
}

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
    int listenfd = accept(sockfd, (sockaddr*)&client, &len);
    if (listenfd < 0) {
        return -1;
    }

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(1);
    assert(epollfd != -1);
    addFd(epollfd, listenfd, true);

    while(1) {
        ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (ret < 0) {
            break;
        }
        lt(events, ret, epollfd, listenfd);
        // et(events, ret, epollfd, listenfd);
    }
    close(listenfd);
    return 0;
}




