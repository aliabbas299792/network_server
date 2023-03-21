#ifndef DEBUG_MEM_OPS
#define DEBUG_MEM_OPS

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <unordered_map>

#define DEBUG_MODE

extern size_t MEM_ALLOCATED;
extern std::unordered_map<uint64_t, size_t> PTR_TO_ALLOC;

void *MALLOC_DEBUG(size_t size);
void FREE_DEBUG(void *ptr);
void MEM_PRINT_DEBUG();

#ifdef DEBUG_MODE
#define MALLOC(size) MALLOC_DEBUG(size)
#define FREE(ptr) FREE_DEBUG(ptr)
#define MEM_PRINT() MEM_PRINT_DEBUG()
#else
#define MALLOC(size) malloc(size)
#define FREE(ptr) free(ptr)
#define MEM_PRINT() ((void)0)
#endif

#endif