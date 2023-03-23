#include "header/debug_mem_ops.hpp"
#include "network_server.hpp"

void network_server::writev_callback(processed_data_vecs write_metadata, uint64_t pfd, uint64_t task_id) {
  auto &task = task_data[task_id];
  auto original_iovs_ptr = reinterpret_cast<iovec *>(task.buff);

  if (write_metadata.op_res_num < 0) {
    switch (errno) {
    case EINTR:
      // for these errors, just try again, otherwise fail
      ev->submit_writev(pfd, write_metadata.iovs, write_metadata.num_vecs, task_id);
      break;
    default: {
      switch (task.op_type) {
      case HTTP_SEND_FILE: {
        http_send_file_writev_finish(pfd, task_id, true);
        break;
      }
      case HTTP_WRITEV:
        callbacks->http_writev_callback(original_iovs_ptr, task.num_iovecs, pfd, true);
        FREE(task.iovs); // writev allocates some memory in get_task(...)
        break;
      case RAW_WRITEV:
        callbacks->raw_writev_callback(original_iovs_ptr, task.num_iovecs, pfd, true);
        FREE(task.iovs); // writev allocates some memory in get_task(...)
        break;
      case WEBSOCKET_WRITEV:
        callbacks->websocket_writev_callback(original_iovs_ptr, task.num_iovecs, pfd, true);
        FREE(task.iovs); // writev allocates some memory in get_task(...)
        break;
      default:
        break;
      }

      // in case of any of these errors, just close the socket
      ev->close_pfd(pfd, task_id);
      return;
    }
    }

    return;
  }

  size_t full_write_size = 0;
  for (size_t i = 0; i < task.num_iovecs; i++) {
    full_write_size += original_iovs_ptr[i].iov_len;
  }

  auto total_progress = task.progress + write_metadata.op_res_num;

  if (total_progress >= full_write_size) {
    switch (task.op_type) {
    case HTTP_SEND_FILE: {
      http_send_file_writev_continue(pfd, task_id);
      return; // all that needs to be done for http_send_file is done in the above procedure
    }
    case HTTP_WRITEV:
      callbacks->http_writev_callback(original_iovs_ptr, task.num_iovecs, pfd);
      ev->close_pfd(pfd, task_id); // task_id freed here
      break;
    case RAW_WRITEV:
      callbacks->raw_writev_callback(original_iovs_ptr, task.num_iovecs, pfd);
      break;
    case WEBSOCKET_WRITEV:
      callbacks->websocket_writev_callback(original_iovs_ptr, task.num_iovecs, pfd);
      break;
    default:
      break;
    }

    FREE(task.iovs); // writev allocates some memory in get_task(...)
    task.iovs = nullptr;
    return;
  }

  // refill data for further writing
  vector_ops_progress(task_id, write_metadata.op_res_num, total_progress);

  ev->submit_writev(pfd, task.iovs, task.num_iovecs, task_id);
}