#include "StateMachine.h"
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>
#include <sys/socket.h>

#define BUFFER_SIZE 4096

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

    int listenfd = ::socket(AF_INET, SOCK_STREAM, 0);
    assert(listenfd >= 0);
    
    int ret = bind(listenfd, (sockaddr*)&address, sizeof(address));
    assert(ret != -1);

    ret = listen (listenfd, 128);
    assert(ret != -1);
    sockaddr_in client;
    socklen_t len = sizeof(client);
    int fd = accept(listenfd, (sockaddr*)&client, &len);
    if (fd < 0) {
        perror("accept error is:");
        return -1;
    }
    else {
        std::string buffer(BUFFER_SIZE, '\0');
        int data_read = 0;
        StateMachine* newStateMachine = new StateMachine(0, 0, 0);
        newStateMachine->setCheckState(StateMachine::CHECK_STATE_REQUESTLINE);
        while(1) {
            int readIndex = newStateMachine->getReadIndex();
            int checkIndex = newStateMachine->getCheckIndex();
            int startLine = newStateMachine->getStartLine();
            data_read = recv(fd, &buffer[readIndex], BUFFER_SIZE - readIndex, 0);
            if (data_read == -1) {
                std::cout << "read failed" << std::endl;
                break;
            }
            else if (data_read == 0) {
                std::cout << "remote client has closed the connection" << std::endl;
                break;
            }
            readIndex += data_read;
            int result = newStateMachine->parse_content(buffer, checkIndex, readIndex, startLine);
            newStateMachine->setReadIndex(readIndex);
            newStateMachine->setCheckIndex(checkIndex);
            newStateMachine->setStartLine(startLine);
            if (result == StateMachine::NO_REQUEST) {
                continue;
            }
            else if (result == StateMachine::GET_REQUEST) {
                send(fd, szret.c_str(), szret.size(), 0);
                break;
            }
            else {
                std::string p = "error";
                send(fd, p.c_str(), p.size(), 0);
                break;
            }
        }
        close(fd);
        delete newStateMachine;
    }
    close(listenfd);
    return 0;
}
