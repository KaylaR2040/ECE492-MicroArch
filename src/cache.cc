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

    // Count the op at this level (this is a *demand* access).
    if (op == Op::Read) ++rd; else ++wr;

    auto it = find(s, tag);
    if (it != s.end()) {
        // Demand hit: move to MRU; if write, mark dirty.
        Line line = *it;
        s.erase(it);
        if (op == Op::Write) line.dirty = true;
        s.push_front(line); // MRU
        return true;
    }

    // Demand miss: tally and fetch/fill
    if (op == Op::Read) ++rmiss; else ++wmiss;
    fill_on_miss(addr, op, idx, tag);
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

bool Cache::writeback(uint32_t addr) {
    // Upper level writes a full dirty block to us. No memory-read to install.
    ++wr; // this is still a "write" to this level
    uint32_t idx = idx_of(addr);
    uint32_t tag = tag_of(addr);
    Set& s = sets[idx];

    auto it = find(s, tag);
    if (it != s.end()) {
        // Hit on writeback: mark dirty but don't bump MRU (not a CPU demand)
        it->dirty = true;
        return true;
    }

    // Miss on writeback: we install without reading memory (we got the data)
    ++wmiss;

    if (s.size() >= ways) {
        evict_victim_and_propagate(idx);
    }

    Line newline;
    newline.valid = true;
    newline.dirty = true;  // obviously dirty (we're writing it back)
    newline.tag   = tag;

    // Writebacks are "cold" here -> insert at LRU (tail). This matches common refs.
    s.push_back(newline);
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
