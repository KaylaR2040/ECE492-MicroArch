#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <vector>
#include <algorithm>

#include "sim.h"

/* ===== helpers ===== */

static inline uint32_t ilog2_u32(uint32_t x) {
    // x is power of two in our use (BLOCKSIZE, #sets)
    uint32_t n = 0;
    while ((1U << n) < x) n++;
    return n;
}

// basename for trace_file printing
static const char* basename_only(const char* p) {
    if (!p) return "";
    const char *s = p, *slash = p;
    while (*s) { if (*s=='/' || *s=='\\') slash = s+1; s++; }
    return slash;
}

typedef struct { uint32_t set; uint32_t tag; } addr_dec_t;

typedef struct {
    bool hit;
    bool evicted_dirty;
    uint32_t evicted_tag;
} alloc_result_t;

/* ===== cache construction & primitives ===== */

static cache_t *cache_create(uint32_t blocksize, uint32_t sizeB, uint32_t assoc, cache_t *next) {
    cache_t *c = (cache_t*)calloc(1, sizeof(cache_t));
    c->blocksize = blocksize;
    c->size_bytes = sizeB;
    c->assoc = assoc;
    c->next = next;

    if (sizeB == 0) return c; // "disabled" cache container (used as memory sentinel)

    c->num_sets = sizeB / (blocksize * assoc);
    c->off_bits = ilog2_u32(blocksize);
    c->idx_bits = ilog2_u32(c->num_sets);
    c->idx_mask = (c->num_sets - 1);

    c->lines = (line_t*)calloc(c->num_sets * assoc, sizeof(line_t));
    return c;
}

static inline line_t* set_base(cache_t *c, uint32_t set) {
    return &c->lines[set * c->assoc];
}

/* Keep LRU bounded: 0 (MRU) .. assoc-1 (LRU) */
static void touch_as_mru(cache_t *c, uint32_t set, uint32_t way_mru) {
    line_t *L = set_base(c, set);
    for (uint32_t i = 0; i < c->assoc; i++) {
        if (!L[i].valid) continue;
        if (i == way_mru) continue;
        if (L[i].lru < c->assoc - 1) L[i].lru++;
    }
    L[way_mru].lru = 0;
}

static int find_hit(cache_t *c, uint32_t set, uint32_t tag) {
    line_t *L = set_base(c, set);
    for (uint32_t i = 0; i < c->assoc; i++) {
        if (L[i].valid && L[i].tag == tag) return (int)i;
    }
    return -1;
}

static int find_invalid(cache_t *c, uint32_t set) {
    line_t *L = set_base(c, set);
    for (uint32_t i = 0; i < c->assoc; i++) {
        if (!L[i].valid) return (int)i;
    }
    return -1;
}

static int find_victim_lru(cache_t *c, uint32_t set) {
    line_t *L = set_base(c, set);
    int v = 0;
    uint32_t best = 0;
    for (uint32_t i = 0; i < c->assoc; i++) {
        if (!L[i].valid) return (int)i;
        if (L[i].lru >= best) { best = L[i].lru; v = (int)i; }
    }
    return v;
}

static alloc_result_t allocate_line(cache_t *c, uint32_t set, uint32_t tag) {
    alloc_result_t R = {0};
    int way = find_invalid(c, set);
    if (way < 0) {
        way = find_victim_lru(c, set);
        line_t *L = set_base(c, set);
        if (L[way].valid && L[way].dirty) {
            R.evicted_dirty = true;
            R.evicted_tag = L[way].tag;
        }
    }
    line_t *L = set_base(c, set);
    L[way].valid = true;
    L[way].dirty = false;
    L[way].tag   = tag;

    touch_as_mru(c, set, (uint32_t)way);
    return R;
}

static addr_dec_t decode(cache_t *c, uint32_t addr) {
    addr_dec_t d;
    d.set = (c->num_sets == 0) ? 0 : ((addr >> c->off_bits) & c->idx_mask);
    d.tag = (addr >> (c->off_bits + c->idx_bits));
    return d;
}

/* Downstream memory traffic accounting */
static void inc_mem_traffic(metrics_t *M, uint64_t x) { M->mem_traffic += x; }

/* ===== cache access =====
   - is_write == true  => a write arriving at this level (CPU store at L1; writeback from upper level at L2)
   - demand_read_into_this_level == true  => this access is a CPU demand fetch into this level (affects read counters)
   - writebacks (is_write && !demand_read_into_this_level) must NOT trigger a read from lower level on miss. */
static bool cache_access(cache_t *c, metrics_t *M, uint32_t addr, bool is_write, bool demand_read_into_this_level) {
    // "Memory" sentinel: every access that reaches here is one memory transaction
    if (c->size_bytes == 0) {
        inc_mem_traffic(M, 1);
        return false;
    }

    addr_dec_t d = decode(c, addr);
    int way = find_hit(c, d.set, d.tag);
    bool is_writeback = is_write && !demand_read_into_this_level;

    // arrivals counters at this level
    if (is_write) c->writes++;
    else if (demand_read_into_this_level) c->reads_demand++;

    // ---- HIT ----
    if (way >= 0) {
        touch_as_mru(c, d.set, (uint32_t)way);
        if (is_write) set_base(c, d.set)[way].dirty = true;  // CPU store or upstream writeback
        return true;
    }

    // ---- MISS ----
    if (is_write) c->write_misses++;
    else if (demand_read_into_this_level) c->read_misses_demand++;

    if (is_writeback) {
        /* -------- WRITEBACK MISS PATH --------
           Install the block here and mark it dirty. Do NOT fetch from below. */
        alloc_result_t ar = allocate_line(c, d.set, d.tag);

        // mark newly installed line dirty
        int w2 = find_hit(c, d.set, d.tag);
        if (w2 >= 0) set_base(c, d.set)[w2].dirty = true;

        // propagate evicted dirty (as a writeback)
        if (ar.evicted_dirty) {
            c->writebacks++;
            uint32_t evict_addr = (ar.evicted_tag << (c->idx_bits + c->off_bits)) | (d.set << c->off_bits);
            if (c->next) (void)cache_access(c->next, M, evict_addr, /*is_write=*/true, /*demand_read_into_this_level=*/false);
            else         inc_mem_traffic(M, 1);
        }

    } else {
        /* -------- DEMAND MISS PATH (read OR write-allocate) --------
           First fetch from next level, then install here. */
        if (c->next) (void)cache_access(c->next, M, addr, /*is_write=*/false, /*demand_read_into_this_level=*/true);
        else         inc_mem_traffic(M, 1);

        alloc_result_t ar = allocate_line(c, d.set, d.tag);

        // propagate evicted dirty (as a writeback)
        if (ar.evicted_dirty) {
            c->writebacks++;
            uint32_t evict_addr = (ar.evicted_tag << (c->idx_bits + c->off_bits)) | (d.set << c->off_bits);
            if (c->next) (void)cache_access(c->next, M, evict_addr, /*is_write=*/true, /*demand_read_into_this_level=*/false);
            else         inc_mem_traffic(M, 1);
        }

        // if original op was a CPU store (write-allocate), mark installed line dirty
        if (is_write) {
            int w2 = find_hit(c, d.set, d.tag);
            if (w2 >= 0) set_base(c, d.set)[w2].dirty = true;
        }
    }

    return false;
}


/* ===== pretty printing ===== */

static void print_cache_contents(const char *title, cache_t *c) {
    printf("===== %s contents =====\n", title);
    if (c->size_bytes == 0) { printf("\n"); return; }

    for (uint32_t set = 0; set < c->num_sets; set++) {
        line_t *L = set_base(c, set);

        struct Item { uint32_t lru, tag; bool dirty; };
        std::vector<Item> v; v.reserve(c->assoc);
        for (uint32_t i = 0; i < c->assoc; i++) if (L[i].valid)
            v.push_back({L[i].lru, L[i].tag, L[i].dirty});
        if (v.empty()) continue;

        std::sort(v.begin(), v.end(),
                  [](const Item& a, const Item& b){ return a.lru < b.lru; }); // MRU first

        printf("set %6u:", set);
        for (const auto& it : v)
            printf(" %8x%s", it.tag, it.dirty ? " D" : "  ");
        printf("\n");
    }
    printf("\n");
}


/* ===== main ===== */

int main (int argc, char *argv[]) {
   FILE *fp;
   char *trace_file;
   cache_params_t P;
   char rw;
   uint32_t addr;

   // Expect exactly 8 args after program name
   if (argc != 9) {
      printf("Error: Expected 8 command-line arguments but was provided %d.\n", (argc - 1));
      return EXIT_FAILURE;
   }

   P.BLOCKSIZE = (uint32_t) atoi(argv[1]);
   P.L1_SIZE   = (uint32_t) atoi(argv[2]);
   P.L1_ASSOC  = (uint32_t) atoi(argv[3]);
   P.L2_SIZE   = (uint32_t) atoi(argv[4]);
   P.L2_ASSOC  = (uint32_t) atoi(argv[5]);
   P.PREF_N    = (uint32_t) atoi(argv[6]);
   P.PREF_M    = (uint32_t) atoi(argv[7]);
   trace_file  = argv[8];

   fp = fopen(trace_file, "r");
   if (!fp) {
      printf("Error: Unable to open file %s\n", trace_file);
      return EXIT_FAILURE;
   }

   /* Build hierarchy: L2 (optional) beneath L1. Prefetch not implemented here (counters kept 0). */
   metrics_t M = {0};
   cache_t *L2 = NULL;
   cache_t *Mem = cache_create(P.BLOCKSIZE, 0, 0, NULL); // sentinel for memory accounting
   if (P.L2_SIZE > 0) {
       L2 = cache_create(P.BLOCKSIZE, P.L2_SIZE, P.L2_ASSOC, Mem);
   }
   cache_t *L1 = cache_create(P.BLOCKSIZE, P.L1_SIZE, P.L1_ASSOC, L2 ? L2 : Mem);

   /* Process trace */
   while (fscanf(fp, " %c %x", &rw, &addr) == 2) {
       if (rw == 'r') {
           M.l1_reads++;
           bool hitL1 = cache_access(L1, &M, addr, false, true);
           if (!hitL1) {
               M.l1_read_misses++;
               // demand read into L2 occurred inside cache_access; we’ll read L2->reads_demand later for 'h'
           }
       } else if (rw == 'w') {
           M.l1_writes++;
           bool hitL1 = cache_access(L1, &M, addr, true, true);
           if (!hitL1) {
               M.l1_write_misses++;
               // write-allocate triggers a demand read into L2; tracked inside cache_access at L2
           }
       } else {
           printf("Error: Unknown request type %c.\n", rw);
           fclose(fp);
           return EXIT_FAILURE;
       }
   }
   fclose(fp);

   /* Pull counters from structures */
   M.l1_writebacks = L1->writebacks;

    // Pull L2 totals directly from the L2 structure (if present)
    if (P.L2_SIZE > 0) {
        M.l2_reads_demand       = L2->reads_demand;
        M.l2_read_misses_demand = L2->read_misses_demand;
        M.l2_writes             = L2->writes;
        M.l2_write_misses       = L2->write_misses;
        M.l2_writebacks         = L2->writebacks;

        // miss rates
        uint64_t l1_refs = M.l1_reads + M.l1_writes;
        M.l1_miss_rate = (l1_refs ? (double)(M.l1_read_misses + M.l1_write_misses) / (double)l1_refs : 0.0);
        M.l2_miss_rate = (M.l2_reads_demand ? (double)M.l2_read_misses_demand / (double)M.l2_reads_demand : 0.0);

        // With L2: q = i + k + m + o + p ; here k=p=0
        M.mem_traffic = M.l2_read_misses_demand + M.l2_write_misses + M.l2_writebacks;

    } else {
        // no L2
        M.l2_reads_demand = M.l2_read_misses_demand = 0;
        M.l2_writes = M.l2_write_misses = M.l2_writebacks = 0;
        M.l2_miss_rate = 0.0;

        uint64_t l1_refs = M.l1_reads + M.l1_writes;
        M.l1_miss_rate = (l1_refs ? (double)(M.l1_read_misses + M.l1_write_misses) / (double)l1_refs : 0.0);

        // No L2: q = b + d + f (+ g=0)
        M.mem_traffic = M.l1_read_misses + M.l1_write_misses + M.l1_writebacks;
    }


   /* ===== Output exactly as spec (sections, labels) ===== */
   // 1) configuration
   printf("===== Simulator configuration =====\n");
   printf("BLOCKSIZE:  %u\n", P.BLOCKSIZE);
   printf("L1_SIZE:    %u\n", P.L1_SIZE);
   printf("L1_ASSOC:   %u\n", P.L1_ASSOC);
   printf("L2_SIZE:    %u\n", P.L2_SIZE);
   printf("L2_ASSOC:   %u\n", P.L2_ASSOC);
   printf("PREF_N:     %u\n", P.PREF_N);
   printf("PREF_M:     %u\n", P.PREF_M);
   printf("trace_file: %s\n", basename_only(trace_file));
   printf("\n");

   // 2) contents
   print_cache_contents("L1", L1);
   if (P.L2_SIZE > 0) print_cache_contents("L2", L2);

   // 3) measurements a–q
   printf("===== Measurements =====\n");
   printf("a. L1 reads:                   %" PRIu64 "\n", M.l1_reads);
   printf("b. L1 read misses:             %" PRIu64 "\n", M.l1_read_misses);
   printf("c. L1 writes:                  %" PRIu64 "\n", M.l1_writes);
   printf("d. L1 write misses:            %" PRIu64 "\n", M.l1_write_misses);
   printf("e. L1 miss rate:               %.4f\n",        M.l1_miss_rate);
   printf("f. L1 writebacks:              %" PRIu64 "\n", M.l1_writebacks);
   printf("g. L1 prefetches:              %" PRIu64 "\n", (uint64_t)0);
   printf("h. L2 reads (demand):          %" PRIu64 "\n", (uint64_t)M.l2_reads_demand);
   printf("i. L2 read misses (demand):    %" PRIu64 "\n", (uint64_t)M.l2_read_misses_demand);
   printf("j. L2 reads (prefetch):        %" PRIu64 "\n", (uint64_t)0);
   printf("k. L2 read misses (prefetch):  %" PRIu64 "\n", (uint64_t)0);
   printf("l. L2 writes:                  %" PRIu64 "\n", (uint64_t)M.l2_writes);
   printf("m. L2 write misses:            %" PRIu64 "\n", (uint64_t)M.l2_write_misses);
   printf("n. L2 miss rate:               %.4f\n",        (P.L2_SIZE ? M.l2_miss_rate : 0.0000));
   printf("o. L2 writebacks:              %" PRIu64 "\n", (uint64_t)M.l2_writebacks);
   printf("p. L2 prefetches:              %" PRIu64 "\n", (uint64_t)0);
   printf("q. memory traffic:             %" PRIu64 "\n", (uint64_t)M.mem_traffic);

   return 0;
}
