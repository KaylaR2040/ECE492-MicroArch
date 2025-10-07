#ifndef SIM_H
#define SIM_H

#include <cstdint>
#include <string>

struct CacheParams {
    uint32_t BLOCKSIZE = 0;
    uint32_t L1_SIZE   = 0;
    uint32_t L1_ASSOC  = 0;
    uint32_t L2_SIZE   = 0;  // 0 disables L2
    uint32_t L2_ASSOC  = 0;  // 0 if no L2
    uint32_t PREF_N    = 0;  // unused for 463
    uint32_t PREF_M    = 0;  // unused for 463
    std::string trace_file;
};

enum class Op : uint8_t { Read, Write };

#endif
