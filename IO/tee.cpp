#include <arpa/inet.h>
#include <cassert>
#include <cstdlib>
#include <fcntl.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, const char* argv[]) {
  if (argc <= 2) {
    return -1;
  }
  int filefd = open(argv[1], O_CREAT | O_WRONLY | O_TRUNC, 0666);
  assert(filefd > 0);
  
  int pipefd_stdout[2];
  int ret = pipe(pipefd_stdout);
  assert(ret != -1);
  
  int pipefd_file[2];
  ret = pipe(pipefd_file);
  
  // 将标准输入的内容输入到管道pipefd_stdout
  ret = splice (STDIN_FILENO, NULL, pipefd_stdout[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
  // 将pipefd_stdout的内容输入到pipefd_stdout
  ret = tee(pipefd_stdout[0], pipefd_file[1], 32768, SPLICE_F_NONBLOCK);
  // 将管道pipefd_file的输出定向到文件描述符filefd上，从而将标准输入的内容写到文件上
  ret = splice(pipefd_file[0], NULL, filefd, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
  // 将pipefd_stdout的输出定向到标准输出，其内容和写入文件的内容完全一致
  ret = splice(pipefd_stdout[0], NULL, STDIN_FILENO, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
  assert(ret != -1);
  
  close(filefd);
  close(pipefd_file[0]);
  close(pipefd_file[1]);
  close(pipefd_stdout[0]);
  close(pipefd_stdout[1]);
  return 0;
}