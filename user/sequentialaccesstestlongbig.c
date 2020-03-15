#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char **argv)
{
  int n0 = nfree();
  printf("free memory init: %d\n", n0);

  int sz = 4096 * 512 * 10;  // size of 10 huge pages
  printf("malloc %d bytes\n", sz);
  char *arr = malloc(sz);
  if (!arr) {
    printf("malloc failed.\n");
    exit(1);
  }
  int n1 = nfree();
  printf("free memory after malloc: %d\n", n1);


  sleep(1);

  printf("sequential access %d bytes\n", sz);
  for (int t = 1; t <= 100; t++) {
    for (int i = 0; i < sz; i++)
      *(arr + i) = t;
    sleep(5);
  }

  int n2 = nfree();
  printf("free memory after access: %d\n", n2);

  printf("free %d bytes\n", sz);
  free(arr);

  exit(0);
}
