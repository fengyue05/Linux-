#include <arpa/inet.h>
#include <cassert>
#include <cstdlib>
#include <fcntl.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char const *argv[])
{
    if (argc <= 3) {
        return -1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);
    const char* path_name = argv[3];

    int filefd = open(path_name, O_RDONLY);
    assert(filefd > 0);

    struct stat stat_buf;
    fstat(filefd, &stat_buf);

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
    if (connfd >= 0) {
        sendfile(connfd, sockfd, NULL, stat_buf.st_size);
        close(connfd);
    }
    close(sockfd);
    return 0;
}
