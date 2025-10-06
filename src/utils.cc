#include "utils.h"
#include <cerrno>
#include <cstring>
#include <iomanip>
#include <iostream>

static inline bool is_pow2(uint32_t x) { return x && !(x & (x-1)); }

bool parse_args(int argc, char* argv[], CacheParams& P, std::string& err) {
    if (argc != 9) { err = "Expected 8 command-line arguments."; return false; }

    P.BLOCKSIZE = (uint32_t) std::stoul(argv[1]);
    P.L1_SIZE   = (uint32_t) std::stoul(argv[2]);
    P.L1_ASSOC  = (uint32_t) std::stoul(argv[3]);
    P.L2_SIZE   = (uint32_t) std::stoul(argv[4]);
    P.L2_ASSOC  = (uint32_t) std::stoul(argv[5]);
    P.PREF_N    = (uint32_t) std::stoul(argv[6]);
    P.PREF_M    = (uint32_t) std::stoul(argv[7]);
    P.trace_file = argv[8];

    // 1. BLOCKSIZE must be a power of 2
    if (!is_pow2(P.BLOCKSIZE)) {
        err = "BLOCKSIZE must be a power of two.";
        return false;
    }

    // 2.  L1 geometry: assoc >=1; size needs to be mult of (block*assoc); sets pow2
    if (P.L1_ASSOC == 0) { err = "L1_ASSOC must be >= 1."; return false; }
    if (P.L1_SIZE == 0 || (P.L1_SIZE % (P.BLOCKSIZE * P.L1_ASSOC)) != 0) {
        err = "L1 geometry invalid: SIZE must be a multiple of BLOCKSIZE*ASSOC.";
        return false;
    }
    uint32_t l1_sets = (P.L1_SIZE / P.BLOCKSIZE) / P.L1_ASSOC;
    if (!is_pow2(l1_sets)) {
        err = "L1 number of sets must be a power of two.";
        return false;
    }

    // 3. L2 geometry (if enabled): assoc >=1; size multiple; sets pow2
    if (P.L2_SIZE == 0) {
        if (P.L2_ASSOC != 0) {
            err = "If L2_SIZE is 0, L2_ASSOC must be 0.";
            return false;
        }
    } else {
        if (P.L2_ASSOC == 0) { err = "L2_ASSOC must be >= 1."; return false; }
        if ((P.L2_SIZE % (P.BLOCKSIZE * P.L2_ASSOC)) != 0) {
            err = "L2 geometry invalid: SIZE must be a multiple of BLOCKSIZE*ASSOC.";
            return false;
        }
        uint32_t l2_sets = (P.L2_SIZE / P.BLOCKSIZE) / P.L2_ASSOC;
        if (!is_pow2(l2_sets)) {
            err = "L2 number of sets must be a power of two.";
            return false;
        }
    }

    return true;
}

bool open_trace(const std::string& path, FILE*& fp) {
    fp = std::fopen(path.c_str(), "r"); // read-only GRADESCOPE
    return fp != nullptr;
}

bool read_trace_line(FILE* fp, Op& op, uint32_t& addr) {
    char c;                // <- was int; must be char for "%c"
    unsigned a;            // KR: REMEMBER "%x" expects unsigned int*
    int n = std::fscanf(fp, " %c %x", &c, &a);
    if (n == 2) {
        op = (c == 'r' || c == 'R') ? Op::Read : Op::Write;
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

static inline double miss_rate(uint64_t r_miss, uint64_t w_miss,
                               uint64_t r, uint64_t w) {
    const double denom = double(r + w);
    if (denom == 0.0) return 0.0;
    return double(r_miss + w_miss) / denom;
}

void print_results(const Cache& L1, const Cache* L2) {
    // Content already printed by caller    
    // Output Stats:
    std::cout << "===== Simulation results (raw) =====\n";
    std::cout << "L1_reads:          " << L1.reads()        << "\n";
    std::cout << "L1_read_misses:    " << L1.read_misses()  << "\n";
    std::cout << "L1_writes:         " << L1.writes()       << "\n";
    std::cout << "L1_write_misses:   " << L1.write_misses() << "\n";
    std::cout << "L1_miss_rate:      " << std::fixed << std::setprecision(4)
              << miss_rate(L1.read_misses(), L1.write_misses(), L1.reads(), L1.writes()) << "\n";
    std::cout.unsetf(std::ios::floatfield);
    std::cout << "L1_writebacks:     " << L1.writebacks()   << "\n";

    if (L2) {
        std::cout << "L2_reads:          " << L2->reads()        << "\n";
        std::cout << "L2_read_misses:    " << L2->read_misses()  << "\n";
        std::cout << "L2_writes:         " << L2->writes()       << "\n";
        std::cout << "L2_write_misses:   " << L2->write_misses() << "\n";
        std::cout << "L2_miss_rate:      " << std::fixed << std::setprecision(4)
                  << miss_rate(L2->read_misses(), L2->write_misses(), L2->reads(), L2->writes()) << "\n";
        std::cout.unsetf(std::ios::floatfield);
        std::cout << "L2_writebacks:     " << L2->writebacks()   << "\n";
        std::cout << "memory_traffic:    " << (L2->memory_reads() + L2->memory_writes()) << "\n";
    } else {
        std::cout << "memory_traffic:    " << (L1.memory_reads() + L1.memory_writes()) << "\n";
    }
    std::cout << "\n";
}
