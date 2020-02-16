#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int base, huge;

  base = nfree();
  huge = hugenfree();

  printf("free base pages %d\n", base);
  printf("free huge pages %d\n", huge);

  exit(0);
}
