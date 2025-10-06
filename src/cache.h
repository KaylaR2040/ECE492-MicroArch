#ifndef CACHE_H
#define CACHE_H

#include <cstdint>
#include <list>
#include <string>
#include <vector>
#include "sim.h"

class Cache {
public:
    Cache(uint32_t size_bytes,
          uint32_t assoc,
          uint32_t block_bytes,
          Cache* next_level = nullptr);

    // Demand access from upper level (or CPU for L1)
    // Implements WBWA + LRU
    // Returns true if hit
    bool access(uint32_t addr, Op op);

    // Dump MRU -> LRU  per set
    void print_contents(const std::string& title) const;

    // Stats getters
    uint64_t reads()        const { return rd; }
    uint64_t read_misses()  const { return rmiss; }
    uint64_t writes()       const { return wr; }
    uint64_t write_misses() const { return wmiss; }
    uint64_t writebacks()   const { return wbs; }

    // Only valid at the last level (or if this is single-level)
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

    // Config
    uint32_t sizeB, ways, blkB;
    uint32_t nsets, offset_bits, index_bits, index_mask;

    // Next level (or nullptr if last level)
    Cache* next;

    // Storage: list per set (front=MRU, back=LRU)
    std::vector<Set> sets;

    // Stats
    uint64_t rd=0, rmiss=0, wr=0, wmiss=0, wbs=0;
    uint64_t mem_rd=0, mem_wr=0; // Increment at last level

    // Helper func.
    inline uint32_t idx_of(uint32_t addr)  const { return (addr >> offset_bits) & index_mask; }
    inline uint32_t tag_of(uint32_t addr)  const { return  (addr >> (offset_bits + index_bits)); }
    inline uint32_t blk_addr(uint32_t tag, uint32_t idx) const {
        return ( (tag << (index_bits + offset_bits)) | (idx << offset_bits) );
    }

    typename Set::iterator find(Set& s, uint32_t tag);

    // Miss handling: fetch from next/memory and allocate; maybe evict victim??
    void fill_on_miss(uint32_t addr, Op op, uint32_t idx, uint32_t tag);
    
    // Evict LRU if needed; if dirty, propagate writeback downstream
    void maybe_evict_and_insert(uint32_t idx, const Line& newline);
};

#endif
