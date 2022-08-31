#ifndef DEBUG_MEM_OPS
#define DEBUG_MEM_OPS

#include "utility.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <unordered_map>

#define DEBUG_MODE

extern size_t MEM_ALLOCATED;
extern std::unordered_map<uint64_t, size_t> PTR_TO_ALLOC;

void *MALLOC(size_t size);
void FREE(void *ptr);
void MEM_PRINT();

#endif