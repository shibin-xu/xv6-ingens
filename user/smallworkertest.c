#include "kernel/types.h"
#include "user/user.h"
#include "kernel/riscv.h"

int
main(int argc, char **argv)
{
    int pids[20];
    int sz = PGSIZE * 400; // not a huge page, not 80%
    int xstatus = 0;
    int i;
    for(i = 0; i < sizeof(pids)/sizeof(pids[0]); i++){
        pids[i] = fork();
        if (pids[i]< -1) {
            printf(" fork %d failled. ERROR\n", i);
            while(i-- > 0) {
                kill(pids[i]);
            }
            exit(1);
        }
        if(pids[i] == 0){
            
            int *arr = malloc(sz);
            // write once
            for (int j = 0; j < sz / sizeof(int); j++) {
                *(arr + j) = pids[i];
            }
            
            printf("start work %d.  ", i);
            //keep on accessing the memory until killed
            for (int k = 0; k < 300; k++) {
                for (int j = 0; j < sz / sizeof(int); j++) {
                    if (*(arr + j) != pids[i]) {
                        xstatus = 1;
                    }
                }   
            }
            free(arr);
            printf("\ndone work %d\n", i); // almost expect no one is done? 
            for(;;) sleep(1000);
        } else {
            printf("forked %d.  ", i);
        }
    }

    sleep(6); // wait for worker to finish.
    printf("\nparent wake up\n");

    while(i-- > 0) {
        kill(pids[i]);
    }

    if (xstatus)
        printf(" ERROR\n");
    else
        printf(" OK\n");
    
    exit(xstatus);
}
