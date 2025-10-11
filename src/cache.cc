#include "cache.h"
#include <cassert>
#include <iomanip>
#include <iostream>

// Compute floor(log2(x)) for x>0.
static inline uint32_t ilog2(uint32_t x) {
    uint32_t n = 0;
    while (x > 1) { x >>= 1; ++n; }
    return n;
}

Cache::Cache(uint32_t size_bytes,
             uint32_t assoc,
             uint32_t block_bytes,
             Cache* next_level)
    : sizeB(size_bytes), ways(assoc), blkB(block_bytes),
      nsets(0), offset_bits(0), index_bits(0), index_mask(0),
      next(next_level),
      rd(0), rmiss(0), wr(0), wmiss(0), wbs(0), mem_rd(0), mem_wr(0) {
    init_geometry();
}

// Initialize cache geometry and data structures based on config params
void Cache::init_geometry() {
    // sanity-check params
    assert(blkB > 0 && (blkB & (blkB - 1)) == 0);         // block pow2
    assert(sizeB > 0 && ways > 0);
    assert((sizeB % blkB) == 0);                          // whole blocks
    assert(((sizeB / blkB) % ways) == 0);                 // whole sets

    nsets = (sizeB / blkB) / ways;
    assert(nsets >= 1 && (nsets & (nsets - 1)) == 0);     // sets pow2

    offset_bits = ilog2(blkB);
    index_bits  = ilog2(nsets);
    index_mask  = (index_bits == 0) ? 0 : ((1u << index_bits) - 1);

    sets.clear();
    sets.resize(nsets);
}

// Find a line with the given tag in the set; Return s.end() if not found
typename Cache::Set::iterator Cache::find(Set& s, uint32_t tag) {
    for (auto it = s.begin(); it != s.end(); ++it) {
        if (it->valid && it->tag == tag) 
        {
            return it;
        }
    }
    return s.end();
}

void Cache::move_to_mru(Set& s, typename Set::iterator it) {
    if (it == s.begin()) 
    {
        return; // MRU.
    }
    Line line = *it;
    s.erase(it);
    s.push_front(line);
}

void Cache::install_line(Set& s, uint32_t tag, bool dirty) {
    Line l;
    l.valid = true;
    l.dirty = dirty;
    l.tag   = tag;
    s.push_front(l); // drop newest at MRU 
}

bool Cache::access(uint32_t addr, Op op) {
    uint32_t idx = idx_of(addr);
    uint32_t tag = tag_of(addr);
    Set& s = sets[idx];

    if (op == Op::Read) ++rd; else ++wr;

    auto it = find(s, tag);
    if (it != s.end()) {
        if (op == Op::Write) it->dirty = true;
        move_to_mru(s, it); // touched add MRU.
        return true;
    }

    if (op == Op::Read) ++rmiss; else ++wmiss;

    if (s.size() >= ways) 
    {
        // Evict first so WB (if exitst) goes out before fetching new line.
        evict_victim_and_propagate(idx);
    }

    if (next) {
        next->access(addr, Op::Read);
    } else {
        ++mem_rd;
    }

    install_line(s, tag, (op == Op::Write));
    return false;
}

bool Cache::writeback(uint32_t addr) {
    ++wr;
    uint32_t idx = idx_of(addr);
    uint32_t tag = tag_of(addr);
    Set& s = sets[idx];

    auto it = find(s, tag);
    if (it != s.end()) {
        it->dirty = true;
        move_to_mru(s, it); // MRU
        return true;
    }

    ++wmiss;
    if (s.size() >= ways)
    {
        evict_victim_and_propagate(idx);
    }

    if (next) 
    {
        next->access(addr, Op::Read);
    } 
    else 
    {
        ++mem_rd; // Count fetch; write-miss traffic shows up in memory totals.
    }
    install_line(s, tag, true);
    return false;
}

void Cache::evict_victim_and_propagate(uint32_t idx) {
    Set& s = sets[idx];
    assert(!s.empty());
    Line victim = s.back();  // LRU
    s.pop_back();

    if (victim.valid && victim.dirty) 
    {
        ++wbs; // WB GENERATED HERE!!! 
        uint32_t victim_addr = blk_addr(victim.tag, idx);
        if (next) next->writeback(victim_addr);
        else
        {
            ++mem_wr;
        } 
    }
}

void Cache::print_contents(const std::string& title) const {
    std::cout << "===== " << title << " =====\n";
    for (uint32_t i = 0; i < nsets; ++i) {
        const Set& s = sets[i];
        std::cout << "set " << std::setw(6) << std::right << i << ":";
        if (s.empty()) 
        { 
            std::cout << "\n"; continue; 
        }
        // READ MRU --> LRU 
        for (auto it = s.begin(); it != s.end(); ++it) {
            if (!it->valid)
            { 
                continue; 
            } 

            std::cout << " " << std::hex << std::nouppercase << it->tag << std::dec;
            
            if (it->dirty) 
            {
                std::cout << " D";
            } 
            else 
            {
                std::cout << "  ";
            }
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}
