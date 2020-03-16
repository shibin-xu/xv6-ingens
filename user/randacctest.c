#include "kernel/types.h"
#include "user/user.h"

unsigned long randstate = 1;
unsigned long randseed = 1;
unsigned int randprimeidx = 0;
unsigned int
rand()
{
  randstate = randstate * 1664525 + 1013904223;
  return randstate;
}

unsigned int
randidx(unsigned int sz) {
    int primes[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 127, 131, 137, 139, 149, 151, 157, 163, 167, 173, 179, 181, 191};
    int p1 = primes[randprimeidx % 43];
    randseed = randseed * p1 % sz;
    randprimeidx += randseed % p1;
    return randseed;
}

int
main(int argc, char **argv)
{

    printf("memory usage before activities: %d\n", nfree());
    int pid = fork();
    if (pid < 0) {
      printf("randacctest: fork failed\n");
      exit(1);
    }
    if (pid == 0) {
        randseed = rand();
        unsigned int sz = 4096 * 512 * 200;  // size of 10 huge pages
        printf("malloc %d bytes\n", sz);
        char *arr = malloc(sz);
        printf("memory usage after malloc: %d\n", nfree());
        if (!arr) {
        printf("malloc failed.\n");
        exit(1);
        }

        sleep(1);
        printf("random write access %d bytes\n", sz);
        for (int i = 0; i < sz; i++) {
            int randv = randidx(sz);
            *(arr + randv) = randv % 7;
        }
        printf("Done write\n");
        sleep(5);

        printf("random read write access %d bytes\n", 3*sz);
        for (int i = 0; i < sz; i++) {
            *(arr + randidx(sz)) = *(arr + randidx(sz));
        }
        printf("Done read write\n");
        sleep(5);

        printf("free %d bytes\n", sz);
        free(arr);
        exit(0);
    } else {
      wait(0);
      sleep(1);
    }
    printf("memory usage before exit: %d\n", nfree());
    exit(0);
}