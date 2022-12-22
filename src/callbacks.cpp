#include "header/debug_mem_ops.hpp"
#include "header/http_response.hpp"
#include "header/metadata.hpp"
#include "network_server.hpp"
#include <cstdint>
#include <sys/inotify.h>
#include <sys/types.h>

void network_server::accept_callback(int listener_pfd, sockaddr_storage *user_data, socklen_t size,
                                     uint64_t pfd, int op_res_num, uint64_t additional_info) {
  // clears the queue, so previous queued stuff doesn't interfere with the accept stuff
  ev->submit_all_queued_sqes();

  if (op_res_num < 0) {
    switch (errno) {
    // for these errors, just try again, otherwise fail
    case ECONNABORTED:
    case EINTR:
      if (ev->submit_accept(listener_pfd) < 0 && !ev->is_dying_or_dead()) {
        // for now just exits if the accept failed
        std::cerr << "\t\teintr failed: (code, pfd, fd): (" << op_res_num << ", " << pfd << ", "
                  << ev->get_pfd_data(pfd).fd << ")\n";
        utility::fatal_error("Accept EINTR resubmit failed");
      }
      break;
    default: {
      if(!ev->is_dying_or_dead()) {
        // there was some other error, in case of accept treat it as fatal for now
        std::string error = "(" + std::string(__FUNCTION__) + ": " + std::to_string(__LINE__);
        error += "), errno: " + std::to_string(errno) + ", op_res_num: " + std::to_string(op_res_num);
        utility::fatal_error(error);
      }

      // in case of any of these errors, just close the socket
      ev->shutdown_and_close_normally(pfd);
    }
    }

    return;
  }

  // also read from the user
  auto buff = (uint8_t *)MALLOC(READ_SIZE);
  auto user_task_id = get_task_buff_op(operation_type::NETWORK_READ, buff, READ_SIZE);
  auto &task = task_data[user_task_id];

  // submits the read, passes the task id as additional info
  // doesn't use queue_read because if this read fails, then submit will fail too
  if (ev->submit_read(pfd, task.buff, task.buff_length, user_task_id) < 0) {
    // if the queueing operation didn't work, end this connection
    FREE(buff);
    free_task(user_task_id);
    ev->shutdown_and_close_normally(pfd);
    std::cerr << "\tinitial read failed: (errno, pfd, fd): (" << errno << ", " << listener_pfd << ", "
              << ev->get_pfd_data(listener_pfd).fd << ")\n";
  }

  // carry on listening, submits everything in the queue with it, not using task_id for this
  if (ev->submit_accept(listener_pfd) < 0 && !ev->is_dying_or_dead()) {
    std::cerr << "\t\tsubmit failed: (errno, pfd, fd): (" << errno << ", " << listener_pfd << ", "
              << ev->get_pfd_data(listener_pfd).fd << ")\n";
    utility::fatal_error("Submit accept normal resubmit failed");
  }
}

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

        if(task.op_type == INOTIFY_READ) {
          ev->shutdown_and_close_normally(pfd);
          return;
        }
      }

      // in case of any of these errors, just close the socket
      ev->shutdown_and_close_normally(pfd);
      // there was some other error, clean up resources, call correct close callback
      application_close_callback(pfd, task_id);
    }
    }

    return;
  }

  if (pfd >= pfd_states.size()) {
    pfd_states.resize(pfd + 1);
  }
  auto &state = pfd_states[pfd];

  if (read_metadata.op_res_num == 0) { // read was 0, socket connection closed
    auto &task = task_data[task_id];
    // only network sockets (and inotify) had network server allocated buffers
    if (task.op_type == INOTIFY_READ || task.op_type == NETWORK_READ || task.op_type == HTTP_POST_READ) {
      FREE(task.buff);

      if(task.op_type == INOTIFY_READ) {
        ev->shutdown_and_close_normally(pfd);
        return;
      }
    }

    if (task.op_type == operation_type::HTTP_SEND_FILE && task.progress == task.buff_length) {
      // http send file, read stage succeeded
      http_send_file_writev_submit(pfd, task_id);
      return;
    } else if (task.op_type == operation_type::HTTP_SEND_FILE) {
      // http send file failed
      http_send_file_read_failed(pfd, task_id);
      // I believe the code below will correctly handle shutting it down
    }

    // if shutdown was called already, and close pfd call fails
    // or if shutdown not called, but shutdown call fails
    // or if it isn't a network socket
    // then shutdown normally and call the close callback
    if ((ev->get_pfd_data(pfd).type != fd_types::NETWORK) ||
        (state.shutdown_done && ev->close_pfd(pfd, task_id) < 0) ||
        (!state.shutdown_done && ev->submit_shutdown(pfd, SHUT_RDWR, task_id) < 0)) {
      ev->shutdown_and_close_normally(pfd);
      application_close_callback(pfd, task_id);
    } else {
      state.last_read_zero = true; // so we can call close in shutdown callback instead
      ev->submit_shutdown(pfd, SHUT_RDWR, task_id);
      // returns here since if we free the task, the marker last_read_zer = true
      // disappears with it
      return;
    }

    free_task(task_id);
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
        if(should_auto_resubmit_read) {
          task.progress = 0;

          // revert to a normal network read
          task.op_type = operation_type::NETWORK_READ;

          state.shutdown_done = false;
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
    if(should_auto_resubmit_read) {
      task.progress = 0;
      state.shutdown_done = false;
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
    inotify_event *e = (inotify_event*)task.buff;
    cache.process_inotify_event(e);
    
    memset(task.buff, 0, task.buff_length);
    int submit_code = ev->submit_read(pfd, task.buff, task.buff_length, task_id);
    if(submit_code < 0) {
      FREE(e);
    }
    break;
  }
  default: {
    // shouldn't get here usually, but this is in case it does
    ev->shutdown_and_close_normally(pfd);
    application_close_callback(pfd, task_id);
    free_task(task_id);
  }
  }
}

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
      ev->shutdown_and_close_normally(pfd);
      // there was some other error
      application_close_callback(pfd, task_id);
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
      close_pfd_gracefully(pfd, task_id); // task_id freed here
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
      ev->shutdown_and_close_normally(pfd);
      // there was some other error
      application_close_callback(pfd, task_id);

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
    close_pfd_gracefully(pfd, task_id); // task_id freed here
    FREE(task.iovs);                    // readv allocates some memory in get_task(...)
    return;
  }

  // refill data for further writing
  vector_ops_progress(task_id, read_metadata.op_res_num, total_progress);

  ev->submit_readv(pfd, task.iovs, task.num_iovecs, task_id);
}

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
        close_pfd_gracefully(pfd, task_id);
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
      ev->shutdown_and_close_normally(pfd);
      // there was some other error
      application_close_callback(pfd, task_id);
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
      std::cout << (int64_t)task.buff << "\n";
      callbacks->http_write_callback(data, pfd);
      close_pfd_gracefully(pfd, task_id);
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
    std::cout << "watch out for this, this is potentially an incorrect freeing of the task\n";
    free_task(task_id); // done writing, task can be freed now
  }
}

void network_server::shutdown_callback(int how, uint64_t pfd, int op_res_num, uint64_t task_id) {
  if (op_res_num < 0) {
    // in case of any of these errors, just close the socket
    ev->shutdown_and_close_normally(pfd);
    // there was some other error

    application_close_callback(pfd, task_id);
    return;
  }

  if (pfd >= pfd_states.size())
    pfd_states.resize(pfd + 1);
  auto &state = pfd_states[pfd];

  // only will be submitting shutdown using SHUT_RDWR
  if (state.last_read_zero) {
    ev->close_pfd(pfd, task_id);
  } else {
    state.shutdown_done = true;
  }
}

void network_server::close_callback(uint64_t pfd, int op_res_num, uint64_t task_id) {
  if (op_res_num < 0) {
    // in case of any of these errors, just close the socket
    ev->shutdown_and_close_normally(pfd);
  }

  application_close_callback(pfd, task_id);
}

void network_server::event_callback(int pfd, int op_res_num, uint64_t additional_info) {
  if (op_res_num < 0) {
    switch (errno) {
    case EINTR:
      // for these errors, just try again, otherwise fail
      if(ev->submit_generic_event(pfd, additional_info) < 0) {
        callbacks->event_trigger_callback(pfd, additional_info, true);
      }
      break;
    default: {
      // in case of any of these errors, just close the socket
      ev->shutdown_and_close_normally(pfd);

      // there was some other error
      callbacks->event_trigger_callback(pfd, additional_info, true);
      return;
    }
    }
  }

  callbacks->event_trigger_callback(pfd, additional_info);
}