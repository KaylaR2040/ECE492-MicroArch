#ifndef UTILS_H
#define UTILS_H

#include <cstdio>
#include <string>
#include "cache.h"
#include "sim.h"

bool parse_args(int argc, char* argv[], CacheParams& P, std::string& err);
bool open_trace(const std::string& path, FILE*& fp);
bool read_trace_line(FILE* fp, Op& op, uint32_t& addr);

void print_config(const CacheParams& P);
void print_results(const Cache& L1, const Cache* L2); // stats + memory traffic

#endif
