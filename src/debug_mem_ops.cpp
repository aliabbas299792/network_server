#include "header/debug_mem_ops.hpp"
#include "header/utility.hpp"

std::unordered_map<uint64_t, size_t> PTR_TO_ALLOC{};
size_t MEM_ALLOCATED{};

void *MALLOC_DEBUG(size_t size) {
  void *ptr = calloc(size, 1);
#ifdef DEBUG_MODE
  MEM_ALLOCATED += size;
  PTR_TO_ALLOC[uint64_t(ptr)] = size;

  LOG_DEBUG(false, "(MALLOC) current memory: " << MEM_ALLOCATED << ", malloc'd: " << size);
#endif
  return ptr;
}

void FREE_DEBUG(void *ptr) {
#ifdef DEBUG_MODE
  size_t size = PTR_TO_ALLOC[(uint64_t)ptr];
  MEM_ALLOCATED -= size;

  LOG_DEBUG(false, "(FREE) current memory: " << MEM_ALLOCATED << ", free'd: " << size);
#endif
  free(ptr);
}

void MEM_PRINT_DEBUG() {
#ifdef DEBUG_MODE
  LOG_DEBUG(false, "(MEM_PRINT) current memory: " << std::to_string(MEM_ALLOCATED));
#endif
}