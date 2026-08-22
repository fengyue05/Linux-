#include <arpa/inet.h>
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/epoll.h>
#include <thread>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_EVENT_NUMBER 1024
#define BUFFER_SIZE 1024

struct fds {
    int epollfd;
    int sockfd;
};

int setNoBlocking (int fd) {
    int old_opration = fcntl(fd, F_GETFL);
    int new_opration = old_opration | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_opration);
    return old_opration;
}

// 将fd上的EPOLLIN和EPOLLET事件注册到epollfd指示的epoll内核事件表中，参数oneshot指定是否注册fd上的EPOLLONTSHOT事件
void addFd (int epollfd, int fd, bool oneshot) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    if (oneshot) {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setNoBlocking(fd);
}

// 重置fd上的事件，这样操作之后，尽管fd上的EPOLLONESHOT事件被注册，但是操作系统仍然会触发fd上的EPOLLIN事件，且只触发一次
void reset_oneshot(int epollfd, int fd) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
} 

// 工作线程
void* worker(void* arg) {
    int sockfd = ((fds*)arg)->sockfd;
    int epollfd = ((fds*)arg)->epollfd;
    char buf[BUFFER_SIZE];
    memset(buf, '\0', BUFFER_SIZE);
    // 循环读取sockfd上的数据，直到遇到EAGAIN
    while(1) {
        int ret = recv(sockfd, buf, BUFFER_SIZE - 1, 0);
        if (ret == 0) {
            close(sockfd);
            break;
        }
        else if (ret < 0) {
            if (errno == EAGAIN) {
                reset_oneshot(epollfd, sockfd);
                break;
            }
        }
        else {
            sleep(5); // 模拟处理数据的过程
        }
    } 
    return nullptr;
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

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        return -1;
    }
    int ret = bind(listenfd, (sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(listenfd, 128);
    assert(ret != -1);

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(1);
    assert(epollfd != -1);

    // 注意，监听socket listened 上是不可以注册EPOLLONESHOT事件的，否则应用程序只能处理一个客户连接！因为后面的客户连接请求将不会触发listenfd上面的EPOLLIN
    addFd(epollfd, listenfd, false);

    while(1) {
        int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if (ret < 0) {
            break;
        }
        for (int i = 0; i < ret; i++) {
            int sockfd = events[i].data.fd;
            if (sockfd == listenfd) {
                sockaddr_in client;
                socklen_t len = sizeof(client);
                int connfd = accept(sockfd, (sockaddr*)&client, &len);
                // 对每个非监听文件描述符都注册EPOLLONESHOT事件
                addFd(epollfd, sockfd, true);
            }
            else if(events[i].events & EPOLLIN) {
                fds fds_for_new_worker;
                fds_for_new_worker.epollfd = epollfd;
                fds_for_new_worker.sockfd = sockfd;
                std::thread thread([fds_for_new_worker](){
                    worker((void*)& fds_for_new_worker);
                });
            }
            else {
                /* code */
            }
        }
    }
    close (listenfd);
    return 0;
}
