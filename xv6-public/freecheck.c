#include "types.h"
#include "stat.h"
#include "user.h"

int
countfree(void)
{
  int fds[2];

  if (pipe(fds) < 0) {
    printf(1, "pipe() failed in countfree()\n");
    exit();
  }

  int pid = fork();
  if (pid < 0) {
    printf(1, "fork failed in countfree()\n");
    exit();
  }

  if (pid == 0) {
    close(fds[0]);

    while (1) {
      uint64 a = (uint64) sbrk(4096);
      if (a == 0xffffffffffffffffULL) {
        break;
      }
      *(char *)(a + 4096 - 1) = 1;
      if (write(fds[1], "x", 1) != 1) {
        printf(1, "write() failed in countfree()\n");
        exit();
      }
    }
    exit();
  }

  close(fds[1]);

  int n = 0;
  while (1) {
    char c;
    int cc = read(fds[0], &c, 1);
    if (cc < 0) {
      printf(1, "read() failed in countfree()\n");
      exit();
    }
    if (cc == 0)
      break;
    n += 1;
  }

  close(fds[0]);
  wait();
  return n;
}

int
main(void)
{
  int free0 = countfree();
  int free1 = countfree();

  if (free1 + 32 < free0) {
    printf(1, "freecheck: FAILED -- lost too many free pages %d (out of %d)\n",
           free1, free0);
  } else {
    printf(1, "freecheck: OK (%d -> %d)\n", free0, free1);
  }
  exit();
}
