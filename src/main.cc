#include <iostream>
#include <memory>
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

    std::unique_ptr<Cache> L2;
    if (P.L2_SIZE > 0) {
        L2 = std::make_unique<Cache>(P.L2_SIZE, P.L2_ASSOC, P.BLOCKSIZE, nullptr);
    }
    Cache L1(P.L1_SIZE, P.L1_ASSOC, P.BLOCKSIZE, L2.get());

    // I drive the hierarchy strictly top-down so every request starts in L1.
    Op op; uint32_t addr;
    while (read_trace_line(fp, op, addr)) {
        L1.access(addr, op);
    }
    std::fclose(fp);

    // Dump contents and measurements
    L1.print_contents("L1 contents");
    if (L2) L2->print_contents("L2 contents");
    print_results(L1, L2.get());
    return 0;
}
