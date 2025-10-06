#include <iostream>
#include "sim.h"
#include "cache.h"
#include "utils.h"

int main(int argc, char* argv[]) {
    CacheParams P;
    std::string err;
    if (!parse_args(argc, argv, P, err)) {
        std::cerr << "Error: " << err << "\n";
        return 1;
    }

    FILE* fp = nullptr;
    if (!open_trace(P.trace_file, fp)) {
        std::cerr << "Error: Unable to open file " << P.trace_file << "\n";
        return 1;
    }

    print_config(P);

    // Build hierarchy
    // L1 policy: typical (promote on hits, demand fills to MRU, WB-miss to MRU)
    Cache L1(P.L1_SIZE, P.L1_ASSOC, P.BLOCKSIZE, nullptr,
             Cache::Policy{/*demand_hit_promote*/true, /*demand_miss_to_mru*/true,
                           /*wb_hit_promote*/false,    /*wb_miss_to_mru*/true});

    Cache* L2ptr = nullptr;
    Cache L2_dummy(1,1,1,nullptr); // harmless placeholder

    if (P.L2_SIZE > 0) {
        L2_dummy = Cache(P.L2_SIZE, P.L2_ASSOC, P.BLOCKSIZE, nullptr,
                         // L2 policy: **no promote on demand hit**; everything else like L1
                         Cache::Policy{/*demand_hit_promote*/false, /*demand_miss_to_mru*/true,
                                       /*wb_hit_promote*/false,      /*wb_miss_to_mru*/true});
        L2ptr = &L2_dummy;
        L1.set_next(L2ptr);
    }

    // Drive simulation
    Op op; uint32_t addr;
    while (read_trace_line(fp, op, addr)) {
        L1.access(addr, op);
    }
    std::fclose(fp);

    // Output contents MRU->LRU and stats
    L1.print_contents("L1 contents");
    if (L2ptr) L2ptr->print_contents("L2 contents");
    print_results(L1, L2ptr);

    return 0;
}
