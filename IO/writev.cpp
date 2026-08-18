#include <arpa/inet.h>
#include <cassert>
#include <iostream>
#include <netinet/in.h>
#include <strings.h>
#include <iostream>
#include <sys/uio.h>
#include <unistd.h>
#include <cstring>
#include <sys/stat.h>
#include <fcntl.h>

#define BUFFER_SIZE 1024

static const char* status_line[2] = {"200 OK", "500 Internal server error"};

int main (int argc, char* argv[]) {
    if (argc <= 2) {
      return -1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);
    const char* file_name = argv[3];

    sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    inet_pton(AF_INET, ip, &address.sin_addr);

    int sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);

    int ret = listen(sockfd, 128);
    assert(ret != -1);

    sockaddr_in client;
    socklen_t len = sizeof(client);
    int connfd = accept(sockfd, (sockaddr*)&client, &len);

    if (connfd < 0) {
        std::cout << "failed" << std::endl;
    } 
    else {
        // 用于保存HTTP应答的状态行，头部字段和一个空行的缓冲区
        char header_buf[BUFFER_SIZE];
        memset(header_buf, '\0', BUFFER_SIZE);
        // 用于存放目标文件内容的应用程序缓存
        char* file_buf;
        // 用于获取目标文件的属性，比如目录，文件大小
        struct stat file_stat;
        // 记录目标文件是否是有效文件
        bool valid = true;
        // 缓冲区header_buf目前已经使用了多少字节的空间
        int len_ret = 0;
        // 第一个参数是文件名路径，第二个参数是把文件的属性全部传入这个结构体里面
        if (stat(file_name, &file_stat) < 0) { // 目标文件不存在
            valid = false;
        } 
        else {
            // file_stat.st_mode 这个里面保存了文件的类型和权限
            if (S_ISDIR(file_stat.st_mode)) { // 目标文件是一个目录
                valid = false;
            }
            else if (file_stat.st_mode & S_IROTH) { // 当前用户有权利读取
                // 动态分配缓存区file_buf，并指定其大小为目标文件的大小file_stat.st_size 加1，然后将目标文件读入缓存区file_buf中
                int fd = open(file_name, O_RDONLY);
                file_buf = new char[file_stat.st_size + 1];
                memset(file_buf, '\0', file_stat.st_size + 1);
                if (read(fd, file_buf, file_stat.st_size + 1) < 0) {
                    valid = false;
                }
            } 
            else {
                valid = false;
            }
        }
        ret = 0;
        // 如果目标文件有效，那么发送正常的HTTP应答
        if (valid) {
            // 下面这个部分将HTTP应答的状态行，“Content-Length”头部字段和一个空行依次加入header_buf中
            ret = snprintf(header_buf, BUFFER_SIZE - 1, "%s %s\r\n", "HTTP/1.1", status_line[0]);
            len_ret += ret;

            ret = snprintf(header_buf + len, BUFFER_SIZE - 1 - len, "Content-Length: %d\r\n", static_cast<int>(file_stat.st_size));
            len += ret;

            ret = snprintf(header_buf, BUFFER_SIZE - len - 1, "%s", "\r\n");
            struct iovec iov[2];
            iov[0].iov_base = header_buf;
            iov[0].iov_len = strlen(header_buf);
            iov[1].iov_base = file_buf;
            iov[1].iov_len = file_stat.st_size;
            ret = ::writev(connfd, iov, 2);
        }
        else {
            ret = snprintf(header_buf, BUFFER_SIZE - 1, "%s %s\r\n", "HTTP/1.1", status_line[1]);
            len += ret;

            ret = snprintf(header_buf + len, BUFFER_SIZE - 1 - len, "%s", "\r\n");
            send(connfd, header_buf, strlen(header_buf), 0);
        }
        close(connfd);
        delete [] file_buf;
    }
    close(sockfd);
    return 0;
}