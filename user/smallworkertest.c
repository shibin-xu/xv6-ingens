#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char **argv)
{
    int pids[20];
    int sz = 4096 * 400; // not a huge page, not 80%
    int xstatus = 0;

    for(int i = 0; i < sizeof(pids)/sizeof(pids[0]); i++){
        if((pids[i] = fork()) == 0){
            int *arr = malloc(sz);
            // write once
            for (int j = 0; j < sz / sizeof(int); j++) {
                *(arr + j) = pids[i];
            }

            // keep on accessing the memory until killed
            for (int k = 0; k < 1000000; k++) {
                for (int j = 0; j < sz / sizeof(int); j++) {
                    if (*(arr + j) != pids[i]) {
                        xstatus = 1;
                    }
                }   
            }
            free(arr);
        }
    }

    sleep(3); // wait for worker to finish.
    for(int i = 0; i < sizeof(pids)/sizeof(pids[0]); i++){
        kill(pids[i]);
    }

    if (xstatus)
        printf(" ERROR\n");
    else
        printf(" OK\n");
    
    exit(xstatus);
}
