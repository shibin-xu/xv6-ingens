//This program is a benchmark for the memory manager system.

#include "kernel/types.h"
#include "user/user.h"
#include <time.h>
#include <limits.h>

//default parameters for benchmark
#define N_TRIALS_DEFAULT 10000
#define PCT_GET_DEFAULT 50
#define PCT_LARGE_DEFAULT 10
#define SMALL_LIMIT_DEFAULT 200
#define LARGE_LIMIT_DEFAULT 20000

//function prototypes
void print_stats(clock_t start_ticks);

uint random_seed = 123456789;

uint rand()
{
    uint a = 1103515245;
    uint c = 12345;
    random_seed = (a * random_seed + c) % UINT_MAX;
    return random_seed;
}

//Bench program to test the memory manager
int main (int argc, char** argv) {
    uint ntrials;  //number of trials
    uint pctget;   //percentage of getmem calls
    uint pctlarge; //percentage of large getmem calls
    uint small_limit;  //upper limit (in bytes) of small getmem calls
    uint large_limit;  //upper limit (in bytes) of large getmem calls

    //parse optional arguments
    ntrials = argc > 1 ? atoi(argv[1]) : N_TRIALS_DEFAULT;
    pctget = argc > 2 ? atoi(argv[2]) : PCT_GET_DEFAULT;
    pctlarge = argc > 3 ? atoi(argv[3]) : PCT_LARGE_DEFAULT;
    small_limit = argc > 4 ? atoi(argv[4]) : SMALL_LIMIT_DEFAULT;
    large_limit = argc > 5 ? atoi(argv[5]) : LARGE_LIMIT_DEFAULT;

    //get random seed from system timer
    random_seed = argc > 6 ? atoi(argv[6]) : 333; //default is the current time (sec)

    //print argument info
    printf("Bench starts with arguments:\n");
    printf("\nNumber of trials=%d\n", ntrials);
    printf("Percentage of malloc calls=%d\n", pctget);
    printf("Percentage of large calls=%d\n", pctlarge);
    printf("Small request limit=%d\n", small_limit);
    printf("Large request limit=%d\n", large_limit);
    printf("Random seed=%d\n\n", random_seed);

    //array to hold all allocated blocks
    pde_t* blk_list = (pde_t*) malloc(sizeof(pde_t) * ntrials);
    //keeps track of number of blocks in list
    uint blk_list_size = 0;

    clock_t start_ticks = uptime();  //capture start time

    uint i;
    for (i=1;i<=ntrials;i++) {

        uint r = rand() % 100 + 1;
        if (r < pctget) {
            r = rand() % 100 + 1;

            if (r < pctlarge) { //large values
                //flip a coin to decide between small and large threshold
                r = rand() % (large_limit - small_limit) + 1;
                blk_list[blk_list_size] = (pde_t) malloc((pde_t) (r + small_limit));
                blk_list_size++;
            } else {            //small values
                //flip a coin to decide between 1 to small threshold
                r = rand() % small_limit + 1;
                blk_list[blk_list_size] = (pde_t) malloc((pde_t) r);
                blk_list_size++;
            }
            //check if free call returned NULL
            if (blk_list[blk_list_size - 1] == 0x00) {
                fprintf(2, "Error: getmem returned NULL pointer. Out of heap space.");
                return 1;
            }

        } else {
            if (blk_list_size) {  //check if there are items in the allocated block array
                r = rand() % blk_list_size;
                free((void*) blk_list[r]);

                //move last element in list to fill the hole
                blk_list[r] = blk_list[blk_list_size - 1];
                blk_list_size--;
            }
        }
    }
    free(blk_list);
    clock_t end_ticks = uptime();  //capture end time
    double elapsed_time = (double) (end_ticks - start_ticks)/CLOCKS_PER_SEC;  //capture elapsed duration
    printf("Total runtime = %f sec\n", elapsed_time);
    return 0;
}

