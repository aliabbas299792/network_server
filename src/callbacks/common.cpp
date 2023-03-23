#include "network_server.hpp"

void network_server::vector_ops_progress(uint64_t task_id, int op_res_num, size_t total_progress) {
  auto &task = task_data[task_id];
  auto original_iovs_ptr = reinterpret_cast<iovec *>(task.buff);

  size_t read_so_far{};
  // so all buffer offsets/lengths are reset
  std::memset(task.iovs, 0, sizeof(iovec) * task.num_iovecs);

  for (size_t i = 0; i < task.num_iovecs; i++) {
    if (read_so_far + original_iovs_ptr[i].iov_len > total_progress) {
      auto offset_in_block = total_progress - read_so_far;

      // set first buffer to correct offset
      task.iovs[0].iov_base = &reinterpret_cast<uint8_t *>(original_iovs_ptr[i].iov_base)[offset_in_block];
      task.iovs[0].iov_len = original_iovs_ptr[i].iov_len - offset_in_block;
      // set remaining buffers to rest of the iovecs
      for (size_t j = 1; j + i < task.num_iovecs; j++) {
        task.iovs[j].iov_base = original_iovs_ptr[i + j].iov_base;
        task.iovs[j].iov_len = original_iovs_ptr[i + j].iov_len;
      }
      break;
    } else if (read_so_far + original_iovs_ptr[i].iov_len == total_progress) {
      // set remaining buffers to rest of the iovecs
      // i++ because this entire iovec has been written, so begin writing from next one
      i++;
      for (int j = 0; j + i < task.num_iovecs; j++) {
        task.iovs[j].iov_base = original_iovs_ptr[i + j].iov_base;
        task.iovs[j].iov_len = original_iovs_ptr[i + j].iov_len;
      }
      break;
    }
    read_so_far += original_iovs_ptr[i].iov_len;
  }

  task.progress += op_res_num;
}
