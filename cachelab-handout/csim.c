#include "cachelab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

typedef int bool;
enum { false, true };

static int hits, misses, evictions;
static int s, E, b;
static unsigned long MAX_N_SET, MAX_N_LINE;
static bool verbose;

//define a struct for a line
typedef struct Block {
    unsigned long time_stamp;
    bool valid; // actually a bool
    unsigned long first_byte_address;
} Block;


int main(int argc, char *argv[])
{
    /*

    define a function to parse the address from trace file

    */
    unsigned long parse_address(char * trace_buffer){
        char address[17];
        int comma_index = 1;
        while (trace_buffer[++comma_index] != ','){
            continue;
        }
        /* get string slice copy of addreess) */
        memcpy(address, &trace_buffer[3], comma_index-3);
        address[comma_index-3] = '\0';
        unsigned long address_num = (unsigned long)strtol(address, NULL, 16);
        return address_num;
    }

    /*

    define a function to get time stamp

    */

    unsigned long get_timestamp(){
        struct timeval tv;
        if (gettimeofday (&tv, NULL) == 0)
            return (unsigned long) (tv.tv_sec * 1000000 + tv.tv_usec);
        else
            return 0;
    }
    /*

    define a function to simulate cache operation

    */
    void cache_operation(Block **Cache, unsigned long address){
        //get offset and set bits
        unsigned long off_set = ((1 << b) -1) & address;
        unsigned long set_num = (address >> b) & ((1 << s)-1);
        //loop over all the blocks to check
        int oldest_block_idx = 0;
        bool hit_or_empty = false;

        for (unsigned long i = 0; i < MAX_N_LINE; i++){
            if (Cache[set_num][i].valid == true){
                //if hits
                if ((address - off_set) == Cache[set_num][i].first_byte_address){
                    hit_or_empty = true;
                    hits++;
                    //update the timestamp for that cache block
                    Cache[set_num][i].time_stamp = get_timestamp();
                    if (verbose) printf("hit ");
                    break;
                }
            }else{
                //if there is an empty slot
                Block new_block = {get_timestamp(), true, address - off_set};
                Cache[set_num][i] = new_block;
                // printf("%d ", i);
                // printf("%lx\n", Cache[set_num][i].first_byte_address);
                hit_or_empty = true;
                misses++;
                if (verbose) printf("miss ");
                break;
            }
            //keep track of the oldest block
            if (i > 0 &&
                Cache[set_num][i].time_stamp < Cache[set_num][oldest_block_idx].time_stamp){
                oldest_block_idx = i;
            }
        }
        //if all the slots are taken and  a miss, LRU policy kicked in
        if (!hit_or_empty){
            //kick out oldest cache
            Block new_block = {get_timestamp(), true, address - off_set};
            Cache[set_num][oldest_block_idx] = new_block;
            misses++;
            evictions++;
            if (verbose) printf("miss eviction ");
        }

    }

    //main body of the simulator
    char* TRACE_FILE_PATH = "";

    //parse the arguments
    size_t optind;
    for (optind = 1; optind < argc && argv[optind][0] == '-'; optind++) {
        switch (argv[optind][1]) {
            case 's': s = atoi(argv[optind + 1]); optind++; break;
            case 'E': E = atoi(argv[optind + 1]); optind++; break;
            case 'b': b = atoi(argv[optind + 1]); optind++; break;
            case 't': TRACE_FILE_PATH = argv[optind + 1]; optind++; break;
            case 'v': verbose = true; break;
            default:
                fprintf(stderr, "Usage: %s [-sEbtv]\n", argv[0]);
                exit(1);
        }

    }
    if (optind < 9){
        fprintf(stderr, "not enought arguments\n");
        exit(1);
    }


    //intialize parameters

    MAX_N_SET = 1 << s;
    MAX_N_LINE = (unsigned long) E;

    hits = 0;
    misses = 0;
    evictions = 0;

    // initialize cache

    Block **Cache = malloc(sizeof(Block *) * MAX_N_SET);


    if(Cache == NULL){
        fprintf(stderr, "unsucessful memory allocation for simulated cache\n");
        exit(1);
    }

    for (int i = 0; i < MAX_N_SET; i++){
        Cache[i] = malloc(sizeof(Block) * MAX_N_LINE);
        if(Cache[i] == NULL){
            fprintf(stderr, "unsucessful memory allocation for simulated cache\n");
            exit(1);
        }
        for(int j =0; j < MAX_N_LINE; j++){
            Block block = {0, false, -1};
            Cache[i][j] = block;
        }
    }

    //read the trace file
    if (strlen(TRACE_FILE_PATH) == 0){
        fprintf(stderr, "no valid trace file path to be read");
        exit(1);
    }

    FILE *fp = fopen(TRACE_FILE_PATH, "r");

    if (fp == NULL){
        fprintf(stderr, "failed to read file\n");
        exit(1);
    }

    char buffer[128];
    while(fgets(buffer, 128, fp) != NULL){
        /* parse the input*/
        //if the first letter is I then ignore
        //strip the trailing \n
        buffer[strcspn(buffer,"\n")] = 0;

        //get the first character
        char first_character = buffer[0];
        if (first_character == 'I'){
            continue;
        }
        //get the operation character (the second character)
        char operation_char = buffer[1];
        //get the address
        unsigned long address = parse_address(buffer);

        if (verbose) printf("%s ", buffer + 1);

        if (operation_char == 'L'){
            cache_operation(Cache, address);
        }else if(operation_char == 'S'){
            cache_operation(Cache, address);
        }else if(operation_char == 'M'){
            cache_operation(Cache, address);
            cache_operation(Cache, address);
        }else{
            continue;
        }
        if (verbose) printf("\n");
    }

    // free memory
    for(int i=0; i<MAX_N_SET; i++)
    {
        // for (int j=0; j<MAX_N_LINE; j++){
        //     printf("%lx ", Cache[i][j].first_byte_address);
        // }
        // printf("\n");
        free(Cache[i]);
    }
    free(Cache);
    Cache = NULL;

    printSummary(hits, misses, evictions);
    return 0;
}
