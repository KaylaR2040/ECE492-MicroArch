#ifndef CACHE_H
#define CACHE_H

#include <cstdint>
#include <list>
#include <string>
#include <vector>
#include "sim.h"

class Cache {
public:
    struct Policy {
        // Casual but precise: knobs for replacement/recency.
        bool demand_hit_promote;
        bool demand_miss_to_mru;
        bool wb_hit_promote;
        bool wb_miss_to_mru;

        // Make Clang happy: explicit default ctor instead of in-class inits.
        Policy(bool dhp = true,
               bool dmtm = true,
               bool whp = false,
               bool wmtm = true)
            : demand_hit_promote(dhp),
              demand_miss_to_mru(dmtm),
              wb_hit_promote(whp),
              wb_miss_to_mru(wmtm) {}
    };

    Cache(uint32_t size_bytes,
          uint32_t assoc,
          uint32_t block_bytes,
          Cache* next_level = nullptr,
          Policy pol = Policy())
        : sizeB(size_bytes), ways(assoc), blkB(block_bytes),
          nsets(0), offset_bits(0), index_bits(0), index_mask(0),
          next(next_level), policy(pol),
          rd(0), rmiss(0), wr(0), wmiss(0), wbs(0), mem_rd(0), mem_wr(0) {
        init_geometry();
    }

    void set_policy(const Policy& p) { policy = p; }

    // Demand access from upper level (WBWA + LRU). Returns true if hit.
    bool access(uint32_t addr, Op op);

    // Upper-level evicted a dirty line -> write it here.
    bool writeback(uint32_t addr);

    void print_contents(const std::string& title) const;

    // Stats
    uint64_t reads()        const { return rd; }
    uint64_t read_misses()  const { return rmiss; }
    uint64_t writes()       const { return wr; }
    uint64_t write_misses() const { return wmiss; }
    uint64_t writebacks()   const { return wbs; }
    uint64_t memory_reads()  const { return mem_rd; }
    uint64_t memory_writes() const { return mem_wr; }

    void set_next(Cache* n) { next = n; }

private:
    struct Line {
        bool valid = false;
        bool dirty = false;
        uint32_t tag = 0;
    };
    using Set = std::list<Line>;

    // Config / geometry
    uint32_t sizeB, ways, blkB;
    uint32_t nsets, offset_bits, index_bits, index_mask;
    Cache* next;

    // Per-instance policy (so L1 vs L2 can differ)
    Policy policy;

    // Storage: MRU at front, LRU at back
    std::vector<Set> sets;

    // Counters
    uint64_t rd, rmiss, wr, wmiss, wbs;
    uint64_t mem_rd, mem_wr;

    void init_geometry();
    inline uint32_t idx_of(uint32_t addr)  const { return (index_bits == 0) ? 0 : ((addr >> offset_bits) & index_mask); }
    inline uint32_t tag_of(uint32_t addr)  const { return  (addr >> (offset_bits + index_bits)); }
    inline uint32_t blk_addr(uint32_t tag, uint32_t idx) const {
        return ( (tag << (index_bits + offset_bits)) | (idx << offset_bits) );
    }

    typename Set::iterator find(Set& s, uint32_t tag);
    void evict_victim_and_propagate(uint32_t idx);
};

#endif
