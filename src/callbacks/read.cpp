#include "header/debug_mem_ops.hpp"
#include "network_server.hpp"

void network_server::read_callback(processed_data read_metadata, uint64_t pfd, uint64_t task_id) {
  if (read_metadata.op_res_num < 0) {
    switch (errno) {
    // for these errors, just try again, otherwise fail
    case EINTR:
      // only if the read submission was successful then break, otherwise just close it
      if (ev->submit_read(pfd, read_metadata.buff, read_metadata.length, task_id) >= 0) {
        break;
      }

    default: {
      switch (task_data[task_id].op_type) {
      case operation_type::HTTP_SEND_FILE: {
        // http send file failed
        http_send_file_read_failed(pfd, task_id);
        break;
      }
      case operation_type::RAW_READ: {
        buff_data data{};
        data.buffer = read_metadata.buff;
        data.size = task_data[task_id].progress;
        callbacks->raw_read_callback(data, pfd);
        break;
      }
      case operation_type::NETWORK_READ: {
        network_read_procedure(pfd, task_id, nullptr, true); // failed the request
      }
      default:
        break;
      }

      auto &task = task_data[task_id];
      if (task.op_type == INOTIFY_READ || task.op_type == NETWORK_READ || task.op_type == HTTP_POST_READ) {
        FREE(task.buff);
      }

      // in case of any of these errors, just close the socket
      ev->close_pfd(pfd, task_id);
    }
    }

    return;
  }

  if (read_metadata.op_res_num == 0) { // read was 0, socket connection closed
    auto &task = task_data[task_id];
    // only network sockets (and inotify) had network server allocated buffers
    if (task.op_type == INOTIFY_READ || task.op_type == NETWORK_READ || task.op_type == HTTP_POST_READ) {
      FREE(task.buff);
      task.buff = nullptr;
    }

    if (task.op_type == operation_type::HTTP_SEND_FILE && task.progress == task.buff_length) {
      // http send file, read stage succeeded
      http_send_file_writev_submit(pfd, task_id);
      return;
    } else if (task.op_type == operation_type::HTTP_SEND_FILE) {
      // http send file failed
      http_send_file_read_failed(pfd, task_id);
    }

    ev->close_pfd(pfd, task_id);
    return;
  }

  buff_data data{};
  // network reads will read once up to READ_SIZE bytes, whereas application reads read as much as they
  // can before returning what was requested
  switch (task_data[task_id].op_type) {
  case operation_type::HTTP_SEND_FILE:
  case operation_type::HTTP_POST_READ:
  case operation_type::RAW_READ: {
    auto &task = task_data[task_id];
    auto total_progress = task.progress + read_metadata.op_res_num;
    if (total_progress < task.buff_length) {
      ev->submit_read(pfd, &task.buff[total_progress], task.buff_length - total_progress, task_id);
      task.progress = total_progress;
      return;
    } else { // finished reading
      if (task.op_type == HTTP_SEND_FILE) {
        // http send file, read stage succeeded
        http_send_file_writev_submit(pfd, task_id);
      } else if (task.op_type == HTTP_POST_READ) {
        // read entire post request
        data.buffer = task_data[task_id].buff;
        data.size = total_progress;

        bool should_auto_resubmit_read = false;
        network_read_procedure(pfd, task_id, &should_auto_resubmit_read, false, data);

        auto &task = task_data[task_id];

        // for HTTP_SEND_FILE, auto_resubmit_read automatically converts the request to NETWORK_READ
        if (should_auto_resubmit_read) {
          task.progress = 0;

          // revert to a normal network read
          task.op_type = operation_type::NETWORK_READ;

          std::memset(task.buff, 0, task.buff_length);

          ev->submit_read(pfd, task.buff, task.buff_length, task_id);
        } else {
          // free this task if we're not auto re-reading
          FREE(task.buff);
          free_task(task_id);
        }

      } else {
        data.buffer = task_data[task_id].buff;
        data.size = total_progress;

        callbacks->raw_read_callback(data, pfd);
        free_task(task_id); // task is completed so task id is freed (we assume user will free the buffer)
      }
    }
    break;
  }
  case operation_type::NETWORK_READ: {
    auto task_progress = task_data[task_id].progress;
    size_t read_this_time = read_metadata.op_res_num; // dealt with case that this is < 0 now
    // populate the data struct with necessary and useful info
    data.buffer = read_metadata.buff;
    data.size = read_this_time + task_progress;

    bool should_auto_resubmit_read = false;
    network_read_procedure(pfd, task_id, &should_auto_resubmit_read, false, data);

    // getting ref to task here since the vector that task refers to may reallocate due to get_task()
    // since it increases in size, so the reference may be incorrect
    // and can cause a segmentation fault
    auto &task = task_data[task_id];

    // auto resubmit read - this is for when you've had a normal HTTP operation
    // and so this is needed to know if the socket has closed the connection (since it will read 0)
    if (should_auto_resubmit_read) {
      task.progress = 0;
      std::memset(task.buff, 0, task.buff_length);

      ev->submit_read(pfd, task.buff, task.buff_length, task_id);
    } else {
      // free this task if we're not auto re-reading
      FREE(task.buff);
      free_task(task_id);
    }
    break;
  }
  case operation_type::INOTIFY_READ: {
    auto &task = task_data[task_id];
    inotify_event *e = (inotify_event *)task.buff;
    cache.process_inotify_event(e);

    memset(task.buff, 0, task.buff_length);
    int submit_code = ev->submit_read(pfd, task.buff, task.buff_length, task_id);
    if (submit_code < 0) {
      FREE(e);
    }
    break;
  }
  default: {
    // shouldn't get here usually, but this is in case it does
    ev->close_pfd(pfd, task_id);
    // application_close_callback(pfd, task_id);
    // free_task(task_id);
  }
  }
}