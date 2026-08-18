#include <iostream>
#include <netinet/in.h>
#include <string>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>
#include <strings.h>
#include <sys/socket.h>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "failed" << std::endl;
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]); 

    sockaddr_in server_address;
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    
    // 转化成二进制，把ip的值填入server_address.sin_addr结构体地址里面
    inet_pton(AF_INET, ip, &server_address.sin_addr);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);
    if (connect(sockfd, (sockaddr*)&server_address, sizeof(server_address))) {
        std::cout << "failed" << std::endl;
    }
    else {
        std::string oob_data = "abc";
        std::string normal_data = "123";
        send(sockfd, normal_data.c_str(), sizeof(normal_data), 0);
        send(sockfd, oob_data.c_str(), sizeof(oob_data), MSG_OOB);
        send(sockfd, normal_data.c_str(), sizeof(normal_data), 0);
    }
    ::close(sockfd);
    return 0;
}