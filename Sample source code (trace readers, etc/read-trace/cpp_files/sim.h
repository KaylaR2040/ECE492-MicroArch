#ifndef SIM_CACHE_H
#define SIM_CACHE_H

#include <vector>
#include <inttypes.h>

typedef 
struct {
   uint32_t BLOCKSIZE;
   uint32_t L1_SIZE;
   uint32_t L1_ASSOC;
   uint32_t L2_SIZE;
   uint32_t L2_ASSOC;
   uint32_t PREF_N;
   uint32_t PREF_M;
} cache_params_t;

// =============================================================================
// DATA STRUCTURES
// =============================================================================

// Single cache line 
struct CacheLine {
    bool valid;              
    bool dirty;             
    uint32_t tag;        
    uint32_t lru_position;  // Position in LRU (0 == most recent)
    
    CacheLine() : valid(false), dirty(false), tag(0), lru_position(0) {}
};

class CacheSet;
class Cache;
class CacheSimulator;

// Manages set-associative caches
class CacheSet {
private:
    std::vector<CacheLine> lines;    // Array of cache lines in this set
    uint32_t associativity;          // Number of ways in this set
    
public:
    // Constructor
    CacheSet(uint32_t assoc);
    
    // Core functionality
    bool findLine(uint32_t tag, uint32_t& way);
    uint32_t findLRUWay();
    void updateLRU(uint32_t way);
    bool insertLine(uint32_t tag, bool& eviction_needed, uint32_t& evicted_tag, bool& evicted_dirty);
    void setDirty(uint32_t way, bool dirty);
    bool isDirty(uint32_t way);
    
    // DEBUG INFO
    void displaySet(uint32_t set_index);
    CacheLine& getLine(uint32_t way) { return lines[way]; }
};

// Main cache for direct-mapped and set-associative config
class Cache {
private:
    uint32_t cache_size;      // (bytes)
    uint32_t block_size;      // (bytes)
    uint32_t associativity;   // Ways per set (1 = direct mapped)
    uint32_t num_sets;        
    std::vector<CacheSet> sets; // Array of cache sets
    
    // Bit field calc 4 addr parsing
    uint32_t offset_bits;     // (bits)
    uint32_t index_bits;      // (bits)
    uint32_t tag_bits;        // (bits)
    
    // Statistics tracking
    uint64_t read_accesses;   //  read requests
    uint64_t write_accesses;  //  write requests
    uint64_t read_hits;       //  read hits
    uint64_t write_hits;      //  write hits
    uint64_t read_misses;     //  read misses
    uint64_t write_misses;    //  write misses
    uint64_t writebacks;      //  # of dirty lines evicted

    // Private helper methods
    void calculateBitFields();
    
public:

    Cache(uint32_t size, uint32_t block_sz, uint32_t assoc);
    
    // Cache access method;  returns: true if hit / false if miss
    bool access(uint32_t address, char rw, bool& writeback_needed, uint32_t& writeback_addr);
    
    // Address extraction (Debug output)
    void extractAddressBits(uint32_t addr, uint32_t& tag, uint32_t& index, uint32_t& offset);
    
    // Methods for: Stats and Display
    void printStats(const char* cache_name);
    void displayContents(const char* cache_name);
    double getMissRate();
    uint64_t getTotalMisses();
    uint64_t getWritebacks() { return writebacks; }
    
    // Getters for cache params
    uint32_t getNumSets() { return num_sets; }
    uint32_t getAssociativity() { return associativity; }
    uint32_t getBlockSize() { return block_size; }
    
    // Getters for stats
    uint64_t getReadAccesses() { return read_accesses; }
    uint64_t getWriteAccesses() { return write_accesses; }
    uint64_t getReadHits() { return read_hits; }
    uint64_t getWriteHits() { return write_hits; }
    uint64_t getReadMisses() { return read_misses; }
    uint64_t getWriteMisses() { return write_misses; }
};

// Manages the L1/L2 hierarchy
class CacheSimulator {
private:
    Cache* L1_cache;          
    Cache* L2_cache;          // nullptr if no L2
    
    // Configuration parameters
    cache_params_t params;    // Copy of sim parameters
    
    // Global statistics
    uint64_t total_accesses;  // Mem references processed
    uint64_t memory_traffic;  // Traffic to main memory
    
    // Debug output control
    bool debug_mode;          // Enable detailed per-access output
    uint64_t access_count;    // Counter for access numbering
    
public:
    // Constructor and destructor
    CacheSimulator(const cache_params_t& params, bool debug = false);
    ~CacheSimulator();
    
    // Main sim method
    void processMemoryAccess(uint32_t address, char rw);
    
    // Output methods
    void printFinalStats();
    void printCacheContents();
    
private:
    // Helpers
    void handleL1Miss(uint32_t address, char rw, bool writeback_needed, uint32_t writeback_addr);
    void handleL2Miss(uint32_t address, char rw, bool writeback_needed, uint32_t writeback_addr);
    void printDebugAccess(uint32_t address, char rw, const char* cache_name, uint32_t tag, uint32_t index, bool hit);
};

#endif
