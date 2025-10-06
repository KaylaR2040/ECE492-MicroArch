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
    // Basic geometry checks. Size/assoc can be "weird"; #sets must be pow2.
    assert(blkB > 0 && (blkB & (blkB - 1)) == 0);       // BLOCKSIZE is pow2
    assert(sizeB > 0 && ways > 0);
    assert((sizeB % blkB) == 0);                        // whole blocks
    assert(((sizeB / blkB) % ways) == 0);               // whole sets

    nsets = (sizeB / blkB) / ways;
    assert(nsets >= 1 && (nsets & (nsets - 1)) == 0);   // SETS pow2

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

    if (op == Op::Read) ++rd; else ++wr;

    auto it = find(s, tag);
    if (it != s.end()) {
        // Demand hit: usually promote to MRU
        if (DEMAND_HIT_PROMOTE) {
            Line line = *it;
            s.erase(it);
            if (op == Op::Write) line.dirty = true;
            s.push_front(line);
        } else {
            if (op == Op::Write) it->dirty = true;
        }
        return true;
    }

    // Demand miss
    if (op == Op::Read) ++rmiss; else ++wmiss;

    // Fetch from next or memory
    if (next) next->access(addr, Op::Read);
    else      ++mem_rd;

    // Evict if needed (propagate dirty victim)
    if (s.size() >= ways) evict_victim_and_propagate(idx);

    // Install demand fill (WBWA; writes come in dirty)
    Line newline;
    newline.valid = true;
    newline.dirty = (op == Op::Write);
    newline.tag   = tag;

    if (DEMAND_MISS_TO_MRU) s.push_front(newline);
    else                    s.push_back(newline);
    return false;
}

bool Cache::writeback(uint32_t addr) {
    ++wr; // it's a write to this level
    uint32_t idx = idx_of(addr);
    uint32_t tag = tag_of(addr);
    Set& s = sets[idx];

    auto it = find(s, tag);
    if (it != s.end()) {
        // Hit on WB: mark dirty, usually no MRU promote
        it->dirty = true;
        if (WB_HIT_PROMOTE) {
            Line line = *it;
            s.erase(it);
            s.push_front(line);
        }
        return true;
    }

    // Miss on WB: no memory read needed; just install (dirty)
    ++wmiss;
    if (s.size() >= ways) evict_victim_and_propagate(idx);

    Line newline;
    newline.valid = true;
    newline.dirty = true;
    newline.tag   = tag;

    // <-- main change: writeback miss installs at MRU
    if (WB_MISS_TO_MRU) s.push_front(newline);
    else                s.push_back(newline);
    return false;
}

void Cache::fill_on_miss(uint32_t addr, Op op, uint32_t idx, uint32_t tag) {
    // Demand miss always fetches from next level (or memory).
    if (next) {
        next->access(addr, Op::Read); // lower level will count its own stats
    } else {
        ++mem_rd;                     // last level goes to memory
    }

    // Allocate here, WBWA, demand fills come in hot -> MRU
    // Evict first if needed (and propagate dirty victim)
    if (sets[idx].size() >= ways) {
        evict_victim_and_propagate(idx);
    }

    Line newline;
    newline.valid = true;
    newline.dirty = (op == Op::Write);
    newline.tag   = tag;

    sets[idx].push_front(newline); // MRU for demand fills
}

void Cache::evict_victim_and_propagate(uint32_t idx) {
    Set& s = sets[idx];
    // Evict LRU
    Line victim = s.back();
    s.pop_back();
    if (victim.valid && victim.dirty) {
        ++wbs; // writeback out of THIS cache
        uint32_t victim_addr = blk_addr(victim.tag, idx);
        if (next) {
            // Hand the dirty block down as a writeback (no memory read in next level either)
            next->writeback(victim_addr);
        } else {
            // Last level: write to memory
            ++mem_wr;
        }
    }
}

void Cache::print_contents(const std::string& title) const {
    std::cout << "===== " << title << " =====" << "\n";
    for (uint32_t i = 0; i < nsets; ++i) {
        const Set& s = sets[i];
        std::cout << "set " << std::setw(6) << std::right << i << ":";
        // MRU -> LRU
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
