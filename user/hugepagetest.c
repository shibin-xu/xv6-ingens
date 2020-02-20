#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char **argv)
{
  int sz = 4096 * 512 * 10;
  printf("malloc %d bytes\n", sz);
  char *arr = malloc(sz);
  // give promo some time to upgrade to huge pages
  sleep(10);

  printf("sequential access %d bytes\n", sz);
  for (int i = 0; i < sz; i++)
    *(arr + i) = 1;

  printf("free %d bytes\n", sz);
  free(arr);

  exit(0);
}
