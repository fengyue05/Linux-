#include <arpa/inet.h>
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <strings.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>

#define USER_LIMIT 5 // 最大用户的限制
#define BUFFER_SIZE 64 
#define FD_LIMIT 65536 // 文件描述符的限制

struct client_data {
    sockaddr_in address;
    char* write_buf;
    char buf[BUFFER_SIZE];
};

int setNoBlocking (int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

int main(int argc, char const *argv[])
{
    if (argc <= 3) {
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

    /*
    创建users数组，分配FD_LIMIT个client_data对象。可以预期：每个可能的socket连接都可以获得这么一个对象，
    并且socket的值可以直接用来索引（作为数组的下标）socket连接对应的client_data对象，这是将socket和客户数据关联的简单而高效的方式
    */ 

    client_data* users = new client_data[FD_LIMIT];
    // 尽管我们分配了足够多的client_data，但是为了提高效率，我们还是要限制用户的数量
    pollfd fds[USER_LIMIT + 1];
    int user_counter = 0;
    // 一般把0留给监听的socket
    for (int i = 1; i <= USER_LIMIT; i++) {
        fds[i].fd = -1;
        fds[i].events = 0;
    }   
    fds[0].fd = sockfd;
    fds[0].events = POLLIN | POLLERR;
    fds[0].revents = 0;

    while (1) {
        ret = poll(fds, user_counter + 1, -1);
        if (ret < 0) {
            break;
        }

        for (int i = 0; i < user_counter + 1; i++) {
            if ((fds[i].fd == sockfd) && (fds[i].revents & EPOLLIN)) {
                sockaddr_in client_address;
                bzero(&client_address, sizeof(client_address));
                socklen_t client_len = sizeof(client_address);
                int connfd = accept(sockfd, (sockaddr*)& client_address, &client_len);
                if (connfd < 0) {
                    continue;
                }
                // 如果请求过多，那么就关闭新到的连接
                if (user_counter >= USER_LIMIT) {
                    const char* info = "too many users\n";
                    std::cout << info;
                    send(connfd, info, strlen(info), 0);
                    close(connfd);
                    continue;
                }
                // 对于新的连接，同时修改fds和users数组。前文已经提到了，users[connfd]对应线连接的客户数据
                user_counter++;
                users[connfd].address = client_address;
                setNoBlocking(connfd);
                fds[user_counter].fd = connfd;
                fds[user_counter].events = POLLIN | POLLRDHUP | POLLERR;
                fds[user_counter].revents = 0;
            }
            else if (fds[i].revents & POLLERR) {
                char errors[100];
                memset(errors, '\0', sizeof(errors));
                socklen_t length = sizeof(length);
                if (getsockopt(fds[i].fd, SOL_SOCKET, SO_ERROR, &errors, &length) < 0) {
                    std::cout << "get socket option failed" << std::endl;
                }
                continue;
            }
            else if (fds[i].revents & POLLRDHUP) {
                // 则客户端关闭连接，则服务器也关闭对应的连接，并将其用户数量-1
                users[fds[i].fd] = users[fds[user_counter].fd]; // 把最后一个文件描述符的数据复制到断开连接的地方
                close(fds[i].fd);
                fds[i] = fds[user_counter]; // 同理也是把最后一个放到我们删除的地方来
                i--; // 这个是因为我们现在的i是一个新的文件描述符，所以我们需要重新遍历
                user_counter--;
                std::cout << "a client left" << std::endl;
            }
            else if (fds[i].revents & POLLIN) {
                int connfd = fds[i].fd;
                memset(users[connfd].buf, '\0', BUFFER_SIZE);
                ret = recv (connfd, users[connfd].buf, BUFFER_SIZE - 1, 0);
                std::cout << users[connfd].buf << std::endl;
                if (ret < 0) {
                    // 如果读的操作出现了错误，则关闭连接
                    if (errno != EAGAIN) {
                        close(connfd);
                        users[fds[i].fd] = users[fds[user_counter].fd];
                        fds[i] = fds[user_counter];
                        i--;
                        user_counter--;
                    }
                }
                else if (ret == 0) {
                    
                }   
                else {
                    // 如果接收到客户的数据，则通知其他的socket连接准备写数据（因为我们这里实现的是广播聊天）
                    for (int j = 1; j <= user_counter; j++) {
                        if (fds[j].fd == connfd) { // 这个就是poll的缺点，要遍历才知道我们具体操作的是哪一个fd
                            continue;
                        }
                        fds[j].events |= ~POLLIN;
                        fds[j].events |= POLLOUT;
                        users[fds[j].fd].write_buf = users[connfd].buf;
                    }
                }
            }
            else if (fds[i].revents & POLLOUT) {
                int connfd = fds[i].fd;
                if (!users[connfd].write_buf) {
                    continue;
                }
                ret = send (connfd, users[connfd].write_buf, strlen(users[connfd].write_buf), 0);
                users[connfd].write_buf = NULL;
                // 写完数据之后就要重新注册fds[i]上的可读事件
                fds[i].events |= POLLIN;
                fds[i].events |= ~POLLOUT;          
            }
        }
    }
    delete [] users;
    close(sockfd);
    return 0;

}