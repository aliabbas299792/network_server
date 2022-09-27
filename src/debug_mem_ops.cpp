#include "../header/debug_mem_ops.hpp"

std::unordered_map<uint64_t, size_t> PTR_TO_ALLOC{};
size_t MEM_ALLOCATED{};

void *MALLOC(size_t size) {
  void *ptr = malloc(size);
#ifdef DEBUG_MODE
  MEM_ALLOCATED += size;
  PTR_TO_ALLOC[uint64_t(ptr)] = size;
  utility::log_helper_function("(MALLOC) current memory: " + std::to_string(MEM_ALLOCATED) +
                                   ", malloc'd: " + std::to_string(size),
                               false);
#endif
  return ptr;
}

void FREE(void *ptr) {
#ifdef DEBUG_MODE
  size_t size = PTR_TO_ALLOC[(uint64_t)ptr];
  MEM_ALLOCATED -= size;
  utility::log_helper_function(
      "(FREE) current memory: " + std::to_string(MEM_ALLOCATED) + ", freed: " + std::to_string(size), false);
#endif
  free(ptr);
}

void MEM_PRINT() {
#ifdef DEBUG_MODE
  utility::log_helper_function("(MEM_PRINT) current memory: " + std::to_string(MEM_ALLOCATED), false);
#endif
}