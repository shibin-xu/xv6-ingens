#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char **argv)
{
  int primes[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29};
  int xstatus = 0;
  printf("test hugepage sequential access: ");
  int sz = 4096 * 512 * 10;
  printf("alloc...");
  int *arr = malloc(sz);
  printf("access...");

  for (int i = 0; i < 10; i++) {
    // give promo some time to upgrade to huge pages
    sleep(1);
    for (int j = 0; j < sz / sizeof(int); j++) {
      if (i && *(arr + j) != j * (i-1) * primes[i-1]) {
        xstatus = 1;
      }
      *(arr + j) = j * i * primes[i];
    }
  }
  printf("free...");
  free(arr);

  if (xstatus)
    printf(" ERROR\n");
  else
    printf(" OK\n");

  exit(xstatus);
}
