#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int send[2], recv[2];
  if (pipe(send) < 0 || pipe(recv) < 0) {
    printf("there is a problem when create a pipe!\n");
    exit(0);
  }
  if (!fork()) {
    char c;
    read(send[0], &c, 1);
    int pid = getpid();
    printf("%d: received ping\n", pid);
    write(recv[1], "a", 1);
  } else {
    write(send[1], "a", 1);
    char c;
    read(recv[0], &c, 1);
    int pid = getpid();
    printf("%d: received pong\n", pid);
  }
  exit(0);
}