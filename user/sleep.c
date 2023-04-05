#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if (argc < 2) {
    printf("must enter a sleep time!\n");
    exit(0);
  }
  int time = atoi(argv[0]);
  sleep(time);
  exit(0);
}