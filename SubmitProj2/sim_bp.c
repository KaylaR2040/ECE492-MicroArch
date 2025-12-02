#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sim_bp.h"

/*  argc holds the number of command line arguments
    argv[] holds the commands themselves

    Example:-
    sim bimodal 6 gcc_trace.txt
    argc = 4
    argv[0] = "sim"
    argv[1] = "bimodal"
    argv[2] = "6"
    ... and so on
*/
int main (int argc, char* argv[])
{
    FILE *FP;               // File handler
    char *trace_file;       // Variable that holds trace file name;
    bp_params params;       // look at sim_bp.h header file for the the definition of struct bp_params
    unsigned long long predictions = 0;
    unsigned long long mispredictions = 0;
    unsigned long M = 0;
    unsigned long N = 0;
    unsigned long long table_size = 0;
    unsigned long long table_mask = 0;
    unsigned long long GHR = 0;
    unsigned long long history_mask = 0;
    unsigned char *PHT = NULL;
    unsigned long long i;
    int is_bimodal = 0;
    int is_gshare = 0;
    char outcome;           // Variable holds branch outcome
    unsigned long int addr; // Variable holds the address read from input file
    
    if (!(argc == 4 || argc == 5 || argc == 7))
    {
        printf("Error: Wrong number of inputs:%d\n", argc-1);
        exit(EXIT_FAILURE);
    }
    
    params.bp_name  = argv[1];
    
    // strtoul() converts char* to unsigned long. It is included in <stdlib.h>
    if(strcmp(params.bp_name, "bimodal") == 0)              // Bimodal
    {
        if(argc != 4)
        {
            printf("Error: %s wrong number of inputs:%d\n", params.bp_name, argc-1);
            exit(EXIT_FAILURE);
        }
        params.M2       = strtoul(argv[2], NULL, 10);
        trace_file      = argv[3];
        M               = params.M2;
        N               = 0;
        is_bimodal      = 1;
        printf("COMMAND\n%s %s %lu %s\n", argv[0], params.bp_name, params.M2, trace_file);
    }
    else if(strcmp(params.bp_name, "gshare") == 0)          // Gshare
    {
        if(argc != 5)
        {
            printf("Error: %s wrong number of inputs:%d\n", params.bp_name, argc-1);
            exit(EXIT_FAILURE);
        }
        params.M1       = strtoul(argv[2], NULL, 10);
        params.N        = strtoul(argv[3], NULL, 10);
        trace_file      = argv[4];
        M               = params.M1;
        N               = params.N;
        is_gshare       = 1;
        printf("COMMAND\n%s %s %lu %lu %s\n", argv[0], params.bp_name, params.M1, params.N, trace_file);

    }
    else if(strcmp(params.bp_name, "hybrid") == 0)          // Hybrid
    {
        if(argc != 7)
        {
            printf("Error: %s wrong number of inputs:%d\n", params.bp_name, argc-1);
            exit(EXIT_FAILURE);
        }
        params.K        = strtoul(argv[2], NULL, 10);
        params.M1       = strtoul(argv[3], NULL, 10);
        params.N        = strtoul(argv[4], NULL, 10);
        params.M2       = strtoul(argv[5], NULL, 10);
        trace_file      = argv[6];
        printf("COMMAND\n%s %s %lu %lu %lu %lu %s\n", argv[0], params.bp_name, params.K, params.M1, params.N, params.M2, trace_file);
        printf("Error: hybrid predictor is not implemented for ECE 463.\n");
        exit(EXIT_FAILURE);

    }
    else
    {
        printf("Error: Wrong branch predictor name:%s\n", params.bp_name);
        exit(EXIT_FAILURE);
    }
    
    // Open trace_file in read mode
    FP = fopen(trace_file, "r");
    if(FP == NULL)
    {
        // Throw error and exit if fopen() failed
        printf("Error: Unable to open file %s\n", trace_file);
        exit(EXIT_FAILURE);
    }

    table_size  = 1ULL << M;
    table_mask  = table_size - 1ULL;
    history_mask = (N == 0) ? 0ULL : ((1ULL << N) - 1ULL);

    PHT = (unsigned char*)malloc(table_size * sizeof(unsigned char));
    if(PHT == NULL)
    {
        printf("Error: Unable to allocate predictor table\n");
        fclose(FP);
        exit(EXIT_FAILURE);
    }
    for(i = 0; i < table_size; ++i)
    {
        PHT[i] = 2; // weakly taken
    }
    
    char str[2];
    while(fscanf(FP, "%lx %s", &addr, str) != EOF)
    {
        
        outcome = str[0];
        // if (outcome == 't')
        //     printf("%lx %s\n", addr, "t");           // Print and test if file is read correctly
        // else if (outcome == 'n')
        //     printf("%lx %s\n", addr, "n");          // Print and test if file is read correctly
        /*************************************
            Add branch predictor code here
        **************************************/
        {
            int actual = (outcome == 't') ? 1 : 0;
            unsigned long long pc_index = (addr >> 2) & table_mask;
            unsigned long long index = pc_index;

            if (N > 0)
            {
                unsigned long long upper_n = pc_index >> (M - N);
                unsigned long long lower_mask = (1ULL << (M - N)) - 1ULL;
                unsigned long long lower_bits = pc_index & lower_mask;
                unsigned long long xor_bits = (upper_n ^ (GHR & history_mask)) & history_mask;
                index = (xor_bits << (M - N)) | lower_bits;
            }

            unsigned char counter = PHT[index];
            int prediction = (counter >= 2);

            predictions++;
            if (prediction != actual)
                mispredictions++;

            if (actual)
            {
                if (counter < 3) counter++;
            }
            else
            {
                if (counter > 0) counter--;
            }
            PHT[index] = counter;

            if (N > 0)
            {
                GHR = (GHR >> 1) | ((unsigned long long)actual << (N - 1));
                GHR &= history_mask;
            }
        }
    }
    fclose(FP);

    printf("OUTPUT\n");
    printf(" number of predictions:    %llu\n", predictions);
    printf(" number of mispredictions: %llu\n", mispredictions);
    printf(" misprediction rate:       %.2f%%\n",
           (predictions == 0) ? 0.0 : (100.0 * (double)mispredictions / (double)predictions));

    if (is_bimodal)
        printf("FINAL BIMODAL CONTENTS\n");
    else if (is_gshare)
        printf("FINAL GSHARE CONTENTS\n");

    for(i = 0; i < table_size; ++i)
    {
        printf(" %llu\t%u\n", i, PHT[i]);
    }

    free(PHT);

    return 0;
}
