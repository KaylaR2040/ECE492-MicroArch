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

    void set_next(Cache* n) { next = n; }

    // Demand access from the CPU or an upper cache (write-back, write-allocate).
    bool access(uint32_t addr, Op op);

    // Dirty block delivered from above.
    bool writeback(uint32_t addr);

    void print_contents(const std::string& title) const;

    // Stats accessors
    uint64_t reads()         const { return rd; }
    uint64_t read_misses()   const { return rmiss; }
    uint64_t writes()        const { return wr; }
    uint64_t write_misses()  const { return wmiss; }
    uint64_t writebacks()    const { return wbs; }
    uint64_t memory_reads()  const { return mem_rd; }
    uint64_t memory_writes() const { return mem_wr; }

private:
    struct Line {
        bool valid = false;
        bool dirty = false;
        uint32_t tag = 0;
    };
    using Set = std::list<Line>;   // I keep MRU lines at the front, LRU lines at the back.

    // configuration / geometry
    uint32_t sizeB, ways, blkB;
    uint32_t nsets, offset_bits, index_bits, index_mask;
    Cache*   next;

    std::vector<Set> sets;

    // counters
    uint64_t rd, rmiss, wr, wmiss, wbs;
    uint64_t mem_rd, mem_wr;

    void init_geometry();
    inline uint32_t idx_of(uint32_t addr) const {
        return (index_bits==0) ? 0 : ((addr >> offset_bits) & index_mask);
    }
    inline uint32_t tag_of(uint32_t addr) const {
        return  (addr >> (offset_bits + index_bits));
    }
    inline uint32_t blk_addr(uint32_t tag, uint32_t idx) const {
        return ( (tag << (index_bits + offset_bits)) | (idx << offset_bits) );
    }

    typename Set::iterator find(Set& s, uint32_t tag);
    void move_to_mru(Set& s, typename Set::iterator it);
    void install_line(Set& s, uint32_t tag, bool dirty);
    void evict_victim_and_propagate(uint32_t idx);
};

#endif
