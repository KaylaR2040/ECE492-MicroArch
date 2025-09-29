#ifndef SIM_CACHE_H
#define SIM_CACHE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
   uint32_t BLOCKSIZE;
   uint32_t L1_SIZE;
   uint32_t L1_ASSOC;
   uint32_t L2_SIZE;
   uint32_t L2_ASSOC;
   uint32_t PREF_N;   // parsed but unused in this implementation
   uint32_t PREF_M;   // parsed but unused in this implementation
} cache_params_t;

/* ===== Basic cache data structures ===== */

typedef struct {
    bool     valid;
    bool     dirty;
    uint32_t tag;
    uint32_t lru;   // 0 = MRU, larger = older; always kept in [0..assoc-1]
} line_t;

typedef struct cache_t cache_t;

struct cache_t {
    // geometry
    uint32_t blocksize;
    uint32_t size_bytes;
    uint32_t assoc;
    uint32_t num_sets;
    uint32_t idx_bits;
    uint32_t off_bits;
    uint32_t idx_mask;

    // storage: sets × assoc
    line_t  *lines;         // flat array [num_sets * assoc]

    // next level (NULL => none; we use a special "Mem" container with size_bytes=0)
    cache_t *next;

    // counters local to this level
    uint64_t reads_demand;          // demand reads arriving to this level
    uint64_t read_misses_demand;
    uint64_t writes;                // writes arriving (CPU stores or upstream writebacks)
    uint64_t write_misses;
    uint64_t writebacks;            // evicted dirty lines sent down
};

typedef struct {
    /* a–q from spec (prefetch counters are 0 here) */
    uint64_t l1_reads;                 // a
    uint64_t l1_read_misses;           // b
    uint64_t l1_writes;                // c
    uint64_t l1_write_misses;          // d
    double   l1_miss_rate;             // e
    uint64_t l1_writebacks;            // f
    uint64_t l1_prefetches;            // g (0)
    uint64_t l2_reads_demand;          // h
    uint64_t l2_read_misses_demand;    // i
    uint64_t l2_reads_pref;            // j (0)
    uint64_t l2_read_misses_pref;      // k (0)
    uint64_t l2_writes;                // l
    uint64_t l2_write_misses;          // m
    double   l2_miss_rate;             // n
    uint64_t l2_writebacks;            // o
    uint64_t l2_prefetches;            // p (0)
    uint64_t mem_traffic;              // q
} metrics_t;

#endif
