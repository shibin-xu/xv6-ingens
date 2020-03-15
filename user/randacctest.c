#include "kernel/types.h"
#include "user/user.h"

unsigned long randstate = 1;
unsigned int
rand()
{
  randstate = randstate * 1664525 + 1013904223;
  return randstate;
}

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

    printf("random write access %d bytes\n", 3*sz);
    for (int i = 0; i < sz; i++) {
        int idx = (rand() % sz);
        *(arr + idx) = 5;
    }
    printf("Done write\n");
    sleep(5);

    printf("random read write access %d bytes\n", 3*sz);
    for (int t = 1; t <= 10; t++) {
        for (int i = 0; i < sz; i++) {
            int idx1 = (rand() % sz);
            int idx2 = (rand() % sz);
            *(arr + idx1) = *(arr + idx2);

        }
        printf("Done read write iter:%d\n", t);
        sleep(5);
    }

    printf("free %d bytes\n", sz);
    free(arr);

    exit(0);
}