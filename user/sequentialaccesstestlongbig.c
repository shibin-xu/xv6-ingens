#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char **argv)
{
  int sz = 4096 * 512 * 10;  // size of 10 huge pages
  printf("malloc %d bytes\n", sz);
  char *arr = malloc(sz);
  if (!arr) {
    printf("malloc failed.\n");
    exit(1);
  }


  sleep(1);

  printf("sequential access %d bytes\n", sz);
  for (int t = 1; t <= 100; t++) {
    for (int i = 0; i < sz; i++)
      *(arr + i) = t;
    sleep(5);
  }

  printf("free %d bytes\n", sz);
  free(arr);

  exit(0);
}
