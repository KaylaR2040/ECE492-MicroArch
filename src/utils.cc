#include "utils.h"
#include <cerrno>
#include <cstring>
#include <iomanip>
#include <iostream>

static inline bool is_pow2(uint32_t x) 
{ 
    return x && !(x & (x-1)); 
}

// Parse and validate CLI
bool parse_args(int argc, char* argv[], CacheParams& P, std::string& err) {
    if (argc != 9) 
    { 
        err = "Expected 8 CLI"; return false; 
    }

    P.BLOCKSIZE = (uint32_t) std::stoul(argv[1]);
    P.L1_SIZE   = (uint32_t) std::stoul(argv[2]);
    P.L1_ASSOC  = (uint32_t) std::stoul(argv[3]);
    P.L2_SIZE   = (uint32_t) std::stoul(argv[4]);
    P.L2_ASSOC  = (uint32_t) std::stoul(argv[5]);
    P.PREF_N    = (uint32_t) std::stoul(argv[6]);
    P.PREF_M    = (uint32_t) std::stoul(argv[7]);
    P.trace_file = argv[8];

    // 1. BLOCKSIZE must be power-of-two
    if (!is_pow2(P.BLOCKSIZE)) 
    { err = "BLOCKSIZE must be a power of two."; return false; }

    // 2. L1 geometry
    if (P.L1_ASSOC == 0) 
    { 
        err = "L1_ASSOC must be >= 1."; return false; 
    }
    if (P.L1_SIZE == 0 || (P.L1_SIZE % (P.BLOCKSIZE * P.L1_ASSOC)) != 0) 
    {
        err = "L1 geometry invalid: SIZE must be a multiple of BLOCKSIZE*ASSOC."; return false;
    }
    else
    {
        uint32_t sets = (P.L1_SIZE / P.BLOCKSIZE) / P.L1_ASSOC;
        if (!is_pow2(sets)) 
        { 
            err = "L1 number of sets must be a power of two."; return false; 
        }
    }

    // 3. L2 geometry (maybe? only if present)
    if (P.L2_SIZE == 0) 
    {
        if (P.L2_ASSOC != 0) 
        { 
            err = "If L2_SIZE is 0, L2_ASSOC must be 0."; return false; 
        }
    } 
    else
    {
        if (P.L2_ASSOC == 0) 
        { 
            err = "L2_ASSOC must be >= 1."; return false; 
        }

        if ((P.L2_SIZE % (P.BLOCKSIZE * P.L2_ASSOC)) != 0) 
        {
            err = "L2 geometry invalid: SIZE must be a multiple of BLOCKSIZE*ASSOC."; return false;
        }

        uint32_t sets = (P.L2_SIZE / P.BLOCKSIZE) / P.L2_ASSOC;

        if (!is_pow2(sets)) 
        { 
            err = "L2 number of sets must be a power of two."; return false; 
        }
    }
    return true;
}

bool open_trace(const std::string& path, FILE*& fp) {
    fp = std::fopen(path.c_str(), "r");  // MAKE READ-ONLLY FOR GRADESCOPE
    return fp != nullptr;
}

bool read_trace_line(FILE* fp, Op& op, uint32_t& addr) {
    char c; unsigned a;
    int n = std::fscanf(fp, " %c %x", &c, &a);
    if (n == 2) {
        op   = (c == 'r' || c == 'R') ? Op::Read : Op::Write;
        addr = static_cast<uint32_t>(a);
        return true;
    }
    return false;
}

void print_config(const CacheParams& P) {
    std::cout << "===== Simulator configuration =====\n";
    std::cout << "BLOCKSIZE:  " << P.BLOCKSIZE  << "\n";
    std::cout << "L1_SIZE:    " << P.L1_SIZE    << "\n";
    std::cout << "L1_ASSOC:   " << P.L1_ASSOC   << "\n";
    std::cout << "L2_SIZE:    " << P.L2_SIZE    << "\n";
    std::cout << "L2_ASSOC:   " << P.L2_ASSOC   << "\n";
    std::cout << "PREF_N:     " << P.PREF_N     << "\n";
    std::cout << "PREF_M:     " << P.PREF_M     << "\n";
    std::cout << "trace_file: " << P.trace_file << "\n\n";
}

static inline double l1_miss_rate(uint64_t r_miss, uint64_t w_miss,
                                  uint64_t r, uint64_t w) {
    const double denom = double(r + w);
    if (denom == 0.0) 
    {
        return 0.0;
    }
    return double(r_miss + w_miss) / denom;
}

static inline double l2_miss_rate(uint64_t rd_miss, uint64_t rd) {
    if (rd == 0) 
    {
        return 0.0;
    }
    return double(rd_miss) / double(rd);
}

void print_results(const Cache& L1, const Cache* L2) {
    std::cout << "===== Measurements =====\n";
    // L1
    std::cout << "a. L1 reads:                   " << L1.reads()        << "\n";
    std::cout << "b. L1 read misses:             " << L1.read_misses()  << "\n";
    std::cout << "c. L1 writes:                  " << L1.writes()       << "\n";
    std::cout << "d. L1 write misses:            " << L1.write_misses() << "\n";
    std::cout << "e. L1 miss rate:               " << std::fixed << std::setprecision(4)
              << l1_miss_rate(L1.read_misses(), L1.write_misses(), L1.reads(), L1.writes()) << "\n";
    std::cout.unsetf(std::ios::floatfield);
    std::cout << "f. L1 writebacks:              " << L1.writebacks()   << "\n";
    std::cout << "g. L1 prefetches:              0\n";

    if (L2) {
        std::cout << "h. L2 reads (demand):          " << L2->reads()        << "\n";
        std::cout << "i. L2 read misses (demand):    " << L2->read_misses()  << "\n";
        std::cout << "j. L2 reads (prefetch):        0\n";
        std::cout << "k. L2 read misses (prefetch):  0\n";
        std::cout << "l. L2 writes:                  " << L2->writes()       << "\n";
        std::cout << "m. L2 write misses:            " << L2->write_misses() << "\n";
        std::cout << "n. L2 miss rate:               " << std::fixed << std::setprecision(4)
                  << l2_miss_rate(L2->read_misses(), L2->reads()) << "\n";
        std::cout.unsetf(std::ios::floatfield);
        std::cout << "o. L2 writebacks:              " << L2->writebacks()   << "\n";
        std::cout << "p. L2 prefetches:              0\n";
        std::cout << "q. memory traffic:             " << (L2->memory_reads() + L2->memory_writes()) << "\n";
    } 
    else 
    {
        std::cout << "h. L2 reads (demand):          0\n";
        std::cout << "i. L2 read misses (demand):    0\n";
        std::cout << "j. L2 reads (prefetch):        0\n";
        std::cout << "k. L2 read misses (prefetch):  0\n";
        std::cout << "l. L2 writes:                  0\n";
        std::cout << "m. L2 write misses:            0\n";
        std::cout << "n. L2 miss rate:               0.0000\n";
        std::cout << "o. L2 writebacks:              0\n";
        std::cout << "p. L2 prefetches:              0\n";
        std::cout << "q. memory traffic:             " << (L1.memory_reads() + L1.memory_writes()) << "\n";
    }
}
