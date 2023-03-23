#include "header/debug_mem_ops.hpp"
#include "network_server.hpp"

void network_server::readv_callback(processed_data_vecs read_metadata, uint64_t pfd, uint64_t task_id) {
  auto &task = task_data[task_id];
  auto original_iovs_ptr = reinterpret_cast<iovec *>(task.buff);

  if (read_metadata.op_res_num < 0) {
    switch (errno) {
    case EINTR:
      // for these errors, just try again, otherwise fail
      ev->submit_readv(pfd, read_metadata.iovs, read_metadata.num_vecs, task_id);
      break;
    default: {
      callbacks->raw_readv_callback(original_iovs_ptr, task.num_iovecs, pfd, true);
      // in case of any of these errors, just close the socket
      ev->close_pfd(pfd, task_id);

      FREE(task.iovs); // readv allocates some memory in get_task(...)
      return;
    }
    }

    return;
  }

  auto total_progress = task.progress + read_metadata.op_res_num;

  size_t full_write_size = 0;
  for (size_t i = 0; i < task.num_iovecs; i++) {
    full_write_size += original_iovs_ptr[i].iov_len;
  }

  if (total_progress >= full_write_size) {
    callbacks->raw_readv_callback(original_iovs_ptr, task.num_iovecs, pfd);
    ev->close_pfd(pfd, task_id); // task_id freed here
    FREE(task.iovs);                    // readv allocates some memory in get_task(...)
    return;
  }

  // refill data for further writing
  vector_ops_progress(task_id, read_metadata.op_res_num, total_progress);

  ev->submit_readv(pfd, task.iovs, task.num_iovecs, task_id);
}