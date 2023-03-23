#include "header/utility.hpp"
#include "network_server.hpp"

void network_server::write_callback(processed_data write_metadata, uint64_t pfd, uint64_t task_id) {
  auto &task = task_data[task_id];

  if (write_metadata.op_res_num < 0) {
    switch (errno) {
    case EINTR:
      // for these errors, just try again, otherwise fail
      ev->submit_write(pfd, write_metadata.buff, write_metadata.length, task_id);
      break;
    default: {
      buff_data data{task.progress, task.buff};
      switch (task.op_type) {
      case HTTP_WRITE:
        callbacks->http_write_callback(data, pfd, true);
        ev->close_pfd(pfd, task_id);
        break;
      case RAW_WRITE:
        callbacks->raw_write_callback(data, pfd, true);
        break;
      case WEBSOCKET_WRITE:
        callbacks->websocket_write_callback(data, pfd, true);
        break;
      default:
        break;
      }

      // in case of any of these errors, just close the socket
      ev->close_pfd(pfd, task_id);
      return;
    }
    }
  }

  auto total_progress = task.progress + write_metadata.op_res_num;

  if (total_progress < task.buff_length) {
    task.progress = total_progress;
    ev->submit_write(pfd, &task.buff[total_progress], task.buff_length - total_progress, task_id);
  } else {
    buff_data data{total_progress, task.buff};

    switch (task.op_type) {
    case HTTP_WRITE:
      PRINT_DEBUG((int64_t)task.buff << " -- task buffer pointer");
      callbacks->http_write_callback(data, pfd);
      ev->close_pfd(pfd, task_id);
      break;
    case RAW_WRITE:
      callbacks->raw_write_callback(data, pfd);
      break;
    case WEBSOCKET_WRITE:
      callbacks->websocket_write_callback(data, pfd);
      break;
    default:
      break;
    }

    PRINT_DEBUG("watch out for this, this is potentially an incorrect freeing of the task");
    free_task(task_id); // done writing, task can be freed now
  }
}