#include "cache.h"
#include <cassert>
#include <iomanip>
#include <iostream>

static inline uint32_t ilog2(uint32_t x) {
    uint32_t n = 0;
    while (x > 1) { x >>= 1; ++n; }
    return n;
}

void Cache::init_geometry() {
    // Basic geometry checks. SIZE/ASSOC can be “weird”; sets must be pow2.
    assert(blkB > 0 && (blkB & (blkB - 1)) == 0);         // BLOCKSIZE is pow2
    assert(sizeB > 0 && ways > 0);
    assert((sizeB % blkB) == 0);                          // whole blocks
    assert(((sizeB / blkB) % ways) == 0);                 // whole sets

    nsets = (sizeB / blkB) / ways;
    assert(nsets >= 1 && (nsets & (nsets - 1)) == 0);     // #sets is pow2

    offset_bits = ilog2(blkB);
    index_bits  = ilog2(nsets);
    index_mask  = (index_bits == 0) ? 0 : ((1u << index_bits) - 1);

    sets.clear();
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
        // Demand hit
        if (op == Op::Write) it->dirty = true;
        if (policy.demand_hit_promote) {
            // Move-to-front (MRU)
            Line line = *it;
            s.erase(it);
            s.push_front(line);
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

    if (policy.demand_miss_to_mru) s.push_front(newline);
    else                           s.push_back(newline);
    return false;
}

bool Cache::writeback(uint32_t addr) {
    ++wr; // it's a write to this level
    uint32_t idx = idx_of(addr);
    uint32_t tag = tag_of(addr);
    Set& s = sets[idx];

    auto it = find(s, tag);
    if (it != s.end()) {
        // Writeback hit: mark dirty; typically no MRU bump
        it->dirty = true;
        if (policy.wb_hit_promote) {
            Line line = *it;
            s.erase(it);
            s.push_front(line);
        }
        return true;
    }

    // Writeback miss: allocate dirty line; no extra memory read.
    ++wmiss;
    if (s.size() >= ways) evict_victim_and_propagate(idx);

    Line newline;
    newline.valid = true;
    newline.dirty = true;
    newline.tag   = tag;

    if (policy.wb_miss_to_mru) s.push_front(newline);
    else                       s.push_back(newline);
    return false;
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
            // Hand dirty block down (no memory read at next level either)
            next->writeback(victim_addr);
        } else {
            // Last level → memory
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
