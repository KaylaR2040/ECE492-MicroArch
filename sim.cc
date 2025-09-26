#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include "sim.h"

// =============================================================================
// -------------------- NEW CACHE SIMULATOR IMPLEMENTATION ---------------------
// =============================================================================



// CacheSet Implementation
// =============================================================================

CacheSet::CacheSet(uint32_t assoc) : associativity(assoc) {
    lines.resize(associativity);

    // Init all lines as inv. w/ LRU positions
    for (uint32_t i = 0; i < associativity; i++) {
        lines[i].valid = false;
        lines[i].dirty = false;
        lines[i].tag = 0;
        lines[i].lru_position = i;  // (0 = MRU)
    }
}

bool CacheSet::findLine(uint32_t tag, uint32_t& way) {
    for (uint32_t i = 0; i < associativity; i++) {
        if (lines[i].valid && lines[i].tag == tag) {
            way = i;
            return true;
        }
    }
    return false;
}

uint32_t CacheSet::findLRUWay() {
    uint32_t lru_way = 0;
    uint32_t max_lru_pos = lines[0].lru_position;
    
    for (uint32_t i = 1; i < associativity; i++) {
        if (lines[i].lru_position > max_lru_pos) {
            max_lru_pos = lines[i].lru_position;
            lru_way = i;
        }
    }
    return lru_way;
}

void CacheSet::updateLRU(uint32_t way) {
    uint32_t old_position = lines[way].lru_position;
    
    // Move all lines with position < old_position up by 1
    for (uint32_t i = 0; i < associativity; i++) {
        if (lines[i].lru_position < old_position) {
            lines[i].lru_position++;
        }
    }
    
    // Set accessed line to MRU (position 0)
    lines[way].lru_position = 0;
}

bool CacheSet::insertLine(uint32_t tag, bool& eviction_needed, uint32_t& evicted_tag, bool& evicted_dirty) {
    eviction_needed = false;
    evicted_dirty = false;
    
    // 1. Find invalid line
    for (uint32_t i = 0; i < associativity; i++) {
        if (!lines[i].valid) {
            lines[i].valid = true;
            lines[i].tag = tag;
            lines[i].dirty = false;
            updateLRU(i);
            return true;
        }
    }
    
    // All lines are valid: Evict LRU
    uint32_t lru_way = findLRUWay();
    eviction_needed = true;
    evicted_tag = lines[lru_way].tag;
    evicted_dirty = lines[lru_way].dirty;
    
    // 2. Replace LRU line
    lines[lru_way].tag = tag;
    lines[lru_way].dirty = false;
    updateLRU(lru_way);
    
    return true;
}  

void CacheSet::setDirty(uint32_t way, bool dirty) {
    lines[way].dirty = dirty;
}

bool CacheSet::isDirty(uint32_t way) {
    return lines[way].dirty;
}

void CacheSet::displaySet(uint32_t set_index) {
    std::cout << "set" << std::setw(6) << set_index << ":";
    
    // Sort by LRU position (0 = MRU, highest = LRU)
    std::vector<uint32_t> ways_by_lru(associativity);
    for (uint32_t i = 0; i < associativity; i++) {
        ways_by_lru[i] = i;
    }
    
    // Sort by LRU position (bub)
    for (uint32_t i = 0; i < associativity - 1; i++) {
        for (uint32_t j = 0; j < associativity - i - 1; j++) {
            if (lines[ways_by_lru[j]].lru_position > lines[ways_by_lru[j+1]].lru_position) {
                uint32_t temp = ways_by_lru[j];
                ways_by_lru[j] = ways_by_lru[j+1];
                ways_by_lru[j+1] = temp;
            }
        }
    }
    
    // Display in LRU order
    for (uint32_t i = 0; i < associativity; i++) {
        uint32_t way = ways_by_lru[i];
        if (lines[way].valid) {
            std::cout << "   " << std::hex << lines[way].tag;
            if (lines[way].dirty) {
                std::cout << " D";
            } else {
                std::cout << "  ";
            }
        }
    }
    std::cout << std::endl;
}

// Cache Implementation
// =============================================================================

Cache::Cache(uint32_t size, uint32_t block_sz, uint32_t assoc) 
    : cache_size(size), block_size(block_sz), associativity(assoc) {
    
    // Calculate # of sets
    num_sets = cache_size / (block_size * associativity);
    
    // Initialize statistics
    read_accesses = 0;
    write_accesses = 0;
    read_hits = 0;
    write_hits = 0;
    read_misses = 0;
    write_misses = 0;
    writebacks = 0;
    
    // Calculate bit fields for address parsing
    calculateBitFields();
    
    // Initialize cache sets
    sets.reserve(num_sets);
    for (uint32_t i = 0; i < num_sets; i++) {
        sets.push_back(CacheSet(associativity));
    }
}

void Cache::calculateBitFields() {
    // Calculate offset bits - log2(block_size)
    offset_bits = 0;
    uint32_t temp = block_size;
    while (temp > 1) {
        temp >>= 1;
        offset_bits++;
    }
    
    // Calculate index bits - log2(num_sets)
    index_bits = 0;
    temp = num_sets;
    while (temp > 1) {
        temp >>= 1;
        index_bits++;
    }
    
    // Tag bits = remaining bits (assuming 32-bit addresses)
    tag_bits = 32 - offset_bits - index_bits;
}

void Cache::extractAddressBits(uint32_t addr, uint32_t& tag, uint32_t& index, uint32_t& offset) {
    offset = addr & ((1 << offset_bits) - 1);
    index = (addr >> offset_bits) & ((1 << index_bits) - 1);
    tag = addr >> (offset_bits + index_bits);
}

bool Cache::access(uint32_t address, char rw, bool& writeback_needed, uint32_t& writeback_addr) {
    writeback_needed = false;
    
    // Extract address components
    uint32_t tag, index, offset;
    extractAddressBits(address, tag, index, offset);
    
    // Update access statistics
    if (rw == 'r') {
        read_accesses++;
    } else {
        write_accesses++;
    }
    
    // Check if line exists in cache
    uint32_t way;
    bool hit = sets[index].findLine(tag, way);
    
    if (hit) {
        // Cache hit
        sets[index].updateLRU(way);
        if (rw == 'r') {
            read_hits++;
        } else {
            write_hits++;
            sets[index].setDirty(way, true);  // Mark as dirty on write
        }
        return true;
    } else {
        // Cache miss
        if (rw == 'r') {
            read_misses++;
        } else {
            write_misses++;
        }
        
        // Insert new line into cache
        bool eviction_needed;
        uint32_t evicted_tag;
        bool evicted_dirty;
        
        sets[index].insertLine(tag, eviction_needed, evicted_tag, evicted_dirty);
        
        // If we evicted a dirty line, need writeback
        if (eviction_needed && evicted_dirty) {
            writeback_needed = true;
            writeback_addr = (evicted_tag << (offset_bits + index_bits)) | (index << offset_bits);
            writebacks++;
        }
        
        // For writes, mark the new line as dirty
        if (rw == 'w') {
            uint32_t new_way;
            sets[index].findLine(tag, new_way);  // Find the line we just inserted
            sets[index].setDirty(new_way, true);
        }
        
        return false;
    }
}

void Cache::printStats(const char* cache_name) {
    // This will be called by CacheSimulator for final statistics output
    // Individual cache stats are not printed separately in this implementation
}

void Cache::displayContents(const char* cache_name) {
    std::cout << "===== " << cache_name << " contents =====" << std::endl;
    for (uint32_t i = 0; i < num_sets; i++) {
        sets[i].displaySet(i);
    }
    std::cout << std::endl;
}

double Cache::getMissRate() {
    uint64_t total_accesses = read_accesses + write_accesses;
    if (total_accesses == 0) return 0.0;
    uint64_t total_misses = read_misses + write_misses;
    return (double)total_misses / (double)total_accesses;
}

uint64_t Cache::getTotalMisses() {
    return read_misses + write_misses;
}

// CacheSimulator Implementation  
// =============================================================================

CacheSimulator::CacheSimulator(const cache_params_t& params, bool debug) 
    : params(params), debug_mode(debug) {
    
    // Initialize statistics
    total_accesses = 0;
    memory_traffic = 0;
    access_count = 0;
    
    // Create L1 cache
    L1_cache = new Cache(params.L1_SIZE, params.BLOCKSIZE, params.L1_ASSOC);
    
    // Create L2 cache if specified
    if (params.L2_SIZE > 0) {
        L2_cache = new Cache(params.L2_SIZE, params.BLOCKSIZE, params.L2_ASSOC);
    } else {
        L2_cache = nullptr;
    }
}

CacheSimulator::~CacheSimulator() {
    delete L1_cache;
    if (L2_cache) {
        delete L2_cache;
    }
}

void CacheSimulator::processMemoryAccess(uint32_t address, char rw) {
    total_accesses++;
    access_count++;
    
    if (debug_mode) {
        std::cout << access_count << "=" << rw << " " << std::hex << address << std::endl;
    }
    
    // Try L1 cache first
    bool writeback_needed = false;
    uint32_t writeback_addr = 0;
    bool l1_hit = L1_cache->access(address, rw, writeback_needed, writeback_addr);
    
    if (debug_mode) {
        uint32_t tag, index, offset;
        L1_cache->extractAddressBits(address, tag, index, offset);
        
        std::cout << "\tL1: " << rw << " " << std::hex << address;
        std::cout << " (tag=" << std::hex << tag << " index=" << std::dec << index << ")" << std::endl;
    }
    
    if (l1_hit) {
        // L1 hit - we're done (except for potential writeback)
        if (writeback_needed) {
            handleL1Miss(writeback_addr, 'w', false, 0);  // Handle writeback to L2/memory
        }
    } else {
        // L1 miss - handle miss and potential writeback
        handleL1Miss(address, rw, writeback_needed, writeback_addr);
    }
}

void CacheSimulator::handleL1Miss(uint32_t address, char rw, bool writeback_needed, uint32_t writeback_addr) {
    // Handle writeback first if needed
    if (writeback_needed) {
        if (L2_cache) {
            bool l2_writeback_needed = false;
            uint32_t l2_writeback_addr = 0;
            L2_cache->access(writeback_addr, 'w', l2_writeback_needed, l2_writeback_addr);
            
            if (l2_writeback_needed) {
                memory_traffic++;  // Writeback to memory
            }
        } else {
            memory_traffic++;  // Direct writeback to memory
        }
    }
    
    // Now handle the original miss
    if (L2_cache) {
        bool l2_writeback_needed = false;
        uint32_t l2_writeback_addr = 0;
        bool l2_hit = L2_cache->access(address, 'r', l2_writeback_needed, l2_writeback_addr);  // Always read from L2
        
        if (l2_hit) {
            // L2 hit - handle potential L2 writeback
            if (l2_writeback_needed) {
                memory_traffic++;  // Writeback to memory
            }
        } else {
            // L2 miss - go to memory
            handleL2Miss(address, rw, l2_writeback_needed, l2_writeback_addr);
        }
    } else {
        // No L2 - go directly to memory
        memory_traffic++;
    }
}

void CacheSimulator::handleL2Miss(uint32_t address, char rw, bool writeback_needed, uint32_t writeback_addr) {
    // Handle L2 writeback if needed
    if (writeback_needed) {
        memory_traffic++;
    }
    
    // L2 miss means we need to go to memory
    memory_traffic++;
}

void CacheSimulator::printFinalStats() {
    std::cout << "===== Measurements =====" << std::endl;
    
    // L1 Statistics (ensure decimal output)
    std::cout << std::dec;  // Set to decimal mode
    std::cout << "a. L1 reads:                   " << L1_cache->getReadAccesses() << std::endl;
    std::cout << "b. L1 read misses:             " << L1_cache->getReadMisses() << std::endl;
    std::cout << "c. L1 writes:                  " << L1_cache->getWriteAccesses() << std::endl;
    std::cout << "d. L1 write misses:            " << L1_cache->getWriteMisses() << std::endl;
    std::cout << "e. L1 miss rate:               " << std::fixed << std::setprecision(4) << L1_cache->getMissRate() << std::endl;
    std::cout << "f. L1 writebacks:              " << L1_cache->getWritebacks() << std::endl;
    std::cout << "g. L1 prefetches:              " << 0 << std::endl;  // No prefetching in this implementation
    
    // L2 Statistics
    if (L2_cache) {
        std::cout << "h. L2 reads (demand):          " << L2_cache->getReadAccesses() + L2_cache->getWriteAccesses() << std::endl;
        std::cout << "i. L2 read misses (demand):    " << L2_cache->getTotalMisses() << std::endl;
        std::cout << "j. L2 reads (prefetch):        " << 0 << std::endl;
        std::cout << "k. L2 read misses (prefetch):  " << 0 << std::endl;
        std::cout << "l. L2 writes:                  " << 0 << std::endl;  // L2 writes are writebacks from L1
        std::cout << "m. L2 write misses:            " << 0 << std::endl;
        std::cout << "n. L2 miss rate:               " << std::fixed << std::setprecision(4) << L2_cache->getMissRate() << std::endl;
        std::cout << "o. L2 writebacks:              " << L2_cache->getWritebacks() << std::endl;
        std::cout << "p. L2 prefetches:              " << 0 << std::endl;
    } else {
        std::cout << "h. L2 reads (demand):          " << 0 << std::endl;
        std::cout << "i. L2 read misses (demand):    " << 0 << std::endl;
        std::cout << "j. L2 reads (prefetch):        " << 0 << std::endl;
        std::cout << "k. L2 read misses (prefetch):  " << 0 << std::endl;
        std::cout << "l. L2 writes:                  " << 0 << std::endl;
        std::cout << "m. L2 write misses:            " << 0 << std::endl;
        std::cout << "n. L2 miss rate:               " << std::fixed << std::setprecision(4) << 0.0000 << std::endl;
        std::cout << "o. L2 writebacks:              " << 0 << std::endl;
        std::cout << "p. L2 prefetches:              " << 0 << std::endl;
    }
    
    // Memory traffic calculation: L1 misses + writebacks - L2 hits (if L2 exists)
    uint64_t calculated_traffic = L1_cache->getTotalMisses() + L1_cache->getWritebacks();
    if (L2_cache) {
        // Subtract L2 hits from traffic since they don't go to memory
        uint64_t l2_accesses = L2_cache->getReadAccesses() + L2_cache->getWriteAccesses();
        uint64_t l2_hits = l2_accesses - L2_cache->getTotalMisses();
        calculated_traffic = L1_cache->getTotalMisses() + L1_cache->getWritebacks() - l2_hits + L2_cache->getWritebacks();
    }
    
    std::cout << "q. memory traffic:             " << calculated_traffic << std::endl;
    std::cout << std::endl;
}

void CacheSimulator::printCacheContents() {
    L1_cache->displayContents("L1");
    if (L2_cache) {
        L2_cache->displayContents("L2");
    }
}

// =============================================================================
// EXISTING MAIN FUNCTION (PRESERVED)
// =============================================================================

/*  "argc" holds the number of command-line arguments.
    "argv[]" holds the arguments themselves.

    Example:
    ./sim 32 8192 4 262144 8 3 10 gcc_trace.txt
    argc = 9
    argv[0] = "./sim"
    argv[1] = "32"
    argv[2] = "8192"
    ... and so on
*/
int main (int argc, char *argv[]) {
   FILE *fp;			// File pointer.
   char *trace_file;		// This variable holds the trace file name.
   cache_params_t params;	// Look at the sim.h header file for the definition of struct cache_params_t.
   char rw;			// This variable holds the request's type (read or write) obtained from the trace.
   uint32_t addr;		// This variable holds the request's address obtained from the trace.
				// The header file <inttypes.h> above defines signed and unsigned integers of various sizes in a machine-agnostic way.  "uint32_t" is an unsigned integer of 32 bits.

   // Exit with an error if the number of command-line arguments is incorrect.
   if (argc != 9) {
      printf("Error: Expected 8 command-line arguments but was provided %d.\n", (argc - 1));
      exit(EXIT_FAILURE);
   }
    
   // "atoi()" (included by <stdlib.h>) converts a string (char *) to an integer (int).
   params.BLOCKSIZE = (uint32_t) atoi(argv[1]);
   params.L1_SIZE   = (uint32_t) atoi(argv[2]);
   params.L1_ASSOC  = (uint32_t) atoi(argv[3]);
   params.L2_SIZE   = (uint32_t) atoi(argv[4]);
   params.L2_ASSOC  = (uint32_t) atoi(argv[5]);
   params.PREF_N    = (uint32_t) atoi(argv[6]);
   params.PREF_M    = (uint32_t) atoi(argv[7]);
   trace_file       = argv[8];

   // Open the trace file for reading.
   fp = fopen(trace_file, "r");
   if (fp == (FILE *) NULL) {
      // Exit with an error if file open failed.
      printf("Error: Unable to open file %s\n", trace_file);
      exit(EXIT_FAILURE);
   }
    
   // Print simulator configuration.
   printf("===== Simulator configuration =====\n");
   printf("BLOCKSIZE:  %u\n", params.BLOCKSIZE);
   printf("L1_SIZE:    %u\n", params.L1_SIZE);
   printf("L1_ASSOC:   %u\n", params.L1_ASSOC);
   printf("L2_SIZE:    %u\n", params.L2_SIZE);
   printf("L2_ASSOC:   %u\n", params.L2_ASSOC);
   printf("PREF_N:     %u\n", params.PREF_N);
   printf("PREF_M:     %u\n", params.PREF_M);
   printf("trace_file: %s\n", trace_file);
   printf("\n");

   // =============================================================================
   // NEW CACHE SIMULATOR INTEGRATION
   // =============================================================================
   
   // Create cache simulator instance
   // Set debug_mode to false for final runs (true for detailed debugging)
   CacheSimulator simulator(params, false);
   
   // Read requests from the trace file and process them through the cache simulator
   while (fscanf(fp, "%c %x\n", &rw, &addr) == 2) {	// Stay in the loop if fscanf() successfully parsed two tokens as specified.
      if (rw == 'r' || rw == 'w') {
         // Process the memory access through our cache simulator
         simulator.processMemoryAccess(addr, rw);
      } else {
         printf("Error: Unknown request type %c.\n", rw);
	 exit(EXIT_FAILURE);
      }
   }
   
   // Close the trace file
   fclose(fp);
   
   // Print final cache contents and statistics
   simulator.printCacheContents();
   simulator.printFinalStats();

   return(0);
}
