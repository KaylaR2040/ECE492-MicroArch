#include "cache.h"
#include <cassert>
#include <cmath>
#include <iomanip>
#include <iostream>

static inline uint32_t ilog2(uint32_t x) { 
    uint32_t n = 0;
    while (x > 1) { x >>= 1; ++n; }
    return n;
}

Cache::Cache(uint32_t size_bytes,
             uint32_t assoc,
             uint32_t block_bytes,
             Cache* next_level)
    : sizeB(size_bytes), ways(assoc), blkB(block_bytes), next(next_level)
{
    assert(blkB > 0 && (blkB & (blkB - 1)) == 0);    // BLOCKSIZE
    assert(sizeB > 0 && ways > 0);
    assert((sizeB % blkB) == 0);                      // whole blocks
    assert(((sizeB / blkB) % ways) == 0);             // whole sets

    nsets = (sizeB / blkB) / ways;
    assert(nsets >= 1 && (nsets & (nsets - 1)) == 0); // SETS pow2

    offset_bits = ilog2(blkB);
    index_bits  = ilog2(nsets);
    index_mask  = (index_bits == 0) ? 0 : ((1u << index_bits) - 1);

    sets.resize(nsets);
}

typename Cache::Set::iterator Cache::find(Set& s, uint32_t tag) {
    for (auto it = s.begin(); it != s.end(); ++it) {
        if (it->valid && it->tag == tag) return it;
    }
    return s.end();
}

bool Cache::access(uint32_t addr, Op op) {
    uint32_t idx = idx_of(addr);
    uint32_t tag = tag_of(addr);
    Set& s = sets[idx];

    // classify op
    if (op == Op::Read) ++rd; else ++wr;

    auto it = find(s, tag);
    if (it != s.end()) {
        // HIT
        // Move to MRU
        Line line = *it;
        s.erase(it);
        // If write, mark dirty
        if (op == Op::Write) line.dirty = true;
        s.push_front(line);
        return true;
    }

    // MISS
    if (op == Op::Read) ++rmiss; else ++wmiss;
    fill_on_miss(addr, op, idx, tag);
    return false;
}

void Cache::fill_on_miss(uint32_t addr, Op op, uint32_t idx, uint32_t tag) {
    // 1. Fetch the block from next level or memory
    if (next) {
        next->access(addr, Op::Read); // always read to fill
    } else {
        ++mem_rd;
    }

    // 2. Allocate in this cache (WBWA)
    Line newline;
    newline.valid = true;
    newline.dirty = (op == Op::Write);
    newline.tag   = tag;

    maybe_evict_and_insert(idx, newline);
}

void Cache::maybe_evict_and_insert(uint32_t idx, const Line& newline) {
    Set& s = sets[idx];

    if (s.size() >= ways) {
        // Evict LRU (back)
        Line victim = s.back();
        s.pop_back();
        if (victim.valid && victim.dirty) {
            ++wbs; // writeback out of THIS cache level
            uint32_t victim_addr = blk_addr(victim.tag, idx);
            if (next) {
                // Propagate writeback downstream
                next->access(victim_addr, Op::Write);
            } else {
                // Last level: write to memory
                ++mem_wr;
            }
        }
    }

    // Insert \n as MRU
    s.push_front(newline);
}

void Cache::print_contents(const std::string& title) const {
    std::cout << "===== " << title << " =====" << "\n";
    for (uint32_t i = 0; i < nsets; ++i) {
        const Set& s = sets[i];
        std::cout << "set " << std::setw(6) << std::right << i << ":";
        // MRU to LRU
        if (s.empty()) { std::cout << "\n"; continue; }
        for (auto it = s.begin(); it != s.end(); ++it) {
            if (!it->valid) continue;
            std::cout << " " << std::hex << std::nouppercase << it->tag << std::dec;
            if (it->dirty) std::cout << " D"; else std::cout << "  ";
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}
