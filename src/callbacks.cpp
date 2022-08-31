#include "../header/debug_mem_ops.hpp"
#include "network_server.hpp"

void network_server::accept_callback(int listener_fd, sockaddr_storage *user_data, socklen_t size,
                                     uint64_t pfd, int op_res_num, uint64_t additional_info) {
  if (op_res_num < 0) {
    switch (errno) {
    // for these errors, just try again, otherwise fail
    case ECONNABORTED:
    case EINTR:
      if (ev->submit_accept(pfd, additional_info) < 0) {
        utility::fatal_error("Accept EINTR resubmit failed");
      }
      break;
    default: {
      // in case of any of these errors, just close the socket
      ev->shutdown_and_close_normally(pfd);

      // there was some other error, in case of accept treat it as fatal for now
      std::string error = "(" + std::string(__FUNCTION__) + ": " + std::to_string(__LINE__);
      error += "), errno: " + std::to_string(errno) + ", op_res_num: " + std::to_string(op_res_num);
      utility::fatal_error(error);
    }
    }

    return;
  }

  // also read from the user
  auto user_task_id = get_task();
  auto &task = task_data[user_task_id];

  auto buff = (uint8_t *)MALLOC(READ_SIZE);
  task.buff = buff;
  task.buff_length = READ_SIZE;
  task.op_type = operation_type::NETWORK_READ; // don't know what it is yet

  // queues up the read, passes the task id as additional info
  if (ev->queue_read(pfd, task.buff, task.buff_length, user_task_id) < 0) {
    // if the queueing operation didn't work, end this connection
    FREE(buff);
    free_task(user_task_id);
    ev->shutdown_and_close_normally(pfd);
  }

  // carry on listening, submits everything in the queue with it, not using task_id for this
  if (ev->submit_accept(listener_fd) < 0) {
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
      // in case of any of these errors, just close the socket
      ev->shutdown_and_close_normally(pfd);
      // there was some other error, clean up resources, call correct close callback
      application_close_callback(pfd, task_id);

      free_task(task_id);
    }
    }

    return;
  }

  if (read_metadata.op_res_num == 0) { // read was 0, socket connection closed
    auto &task = task_data[task_id];
    // if shutdown was called already, and close pfd call fails
    // or if shutdown not called, but shutdown call fails
    // or if it isn't a network socket
    // then shutdown normally and call the close callback
    if ((ev->get_pfd_data(pfd).type != fd_types::NETWORK) || (task.shutdown && ev->close_pfd(pfd) < 0) ||
        (!task.shutdown && ev->submit_shutdown(pfd, SHUT_RDWR) < 0)) {
      ev->shutdown_and_close_normally(pfd);
      application_close_callback(pfd, task_id);
    } else {
      task.last_read_zero = true; // so we can call close in shutdown callback instead
    }

    // only network sockets had network server allocated buffers
    if (task.op_type == operation_type::NETWORK_READ) {
      FREE(task.buff);
    }

    free_task(task_id);
    return;
  } else {
    task_data[task_id].last_read_zero = false;
  }

  buff_data data{};

  // network reads will read once up to READ_SIZE bytes, whereas application reads read as much as they can
  // before returning what was requested
  switch (task_data[task_id].op_type) {
  case operation_type::RAW_READ: {
    auto &task = task_data[task_id];
    auto total_progress = task.progress + read_metadata.op_res_num;

    if (total_progress < task.buff_length) {
      ev->submit_read(pfd, &task.buff[total_progress], task.buff_length - total_progress, task_id);
      task.progress = total_progress;
      return;
    } else { // finished reading
      data.buffer = read_metadata.buff;
      data.size = total_progress;

      free_task(task_id); // task is completed so task id is freed (we assume user will free the buffer)
    }
    break;
  }
  case operation_type::NETWORK_READ: {
    size_t read_this_time = read_metadata.op_res_num; // dealt with case that this is < 0 now
    // populate the data struct with necessary and useful info
    data.buffer = read_metadata.buff;
    data.size = read_this_time;

    network_read_procedure(pfd, data);

    // reuse task since we're just going to read READ_SIZE bytes again
    // getting ref to task here since the vector that task refers to may reallocate due to get_task()
    // since it increases in size, so the reference may be incorrect
    // and can cause a segmentation fault
    auto &task = task_data[task_id];
    task.progress = 0;
    task.shutdown = false;
    std::memset(task.buff, 0, READ_SIZE);

    ev->submit_read(pfd, task.buff, READ_SIZE, task_id);
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

  if (write_metadata.op_res_num < 0) {
    switch (errno) {
    case EINTR:
      // for these errors, just try again, otherwise fail
      ev->submit_writev(pfd, write_metadata.iovs, write_metadata.num_vecs, task_id);
      break;
    default: {
      // in case of any of these errors, just close the socket
      ev->shutdown_and_close_normally(pfd);
      // there was some other error
      application_close_callback(pfd, task_id);
      return;
    }
    }
  }

  auto total_progress = task.progress + write_metadata.op_res_num;

  auto buff_iovecs = reinterpret_cast<iovec *>(task.buff);
  size_t written{};

  if (total_progress >= task.buff_length) {
    switch (task.op_type) {
    case HTTP_WRITEV:
      callbacks->http_writev_callback(task.iovs, task.num_iovecs, pfd);
      break;
    case RAW_WRITEV:
      callbacks->raw_writev_callback(task.iovs, task.num_iovecs, pfd);
      break;
    case WEBSOCKET_WRITEV:
      callbacks->websocket_writev_callback(task.iovs, task.num_iovecs, pfd);
      break;
    default:
      break;
    }

    close_pfd_gracefully(pfd, task_id);
    FREE(task.iovs); // writev allocates some memory
    return;
  }

  // so all buffer offsets/lengths are reset
  std::memset(buff_iovecs, 0, sizeof(iovec) * task.num_iovecs);

  for (size_t i = 0; i < task.num_iovecs; i++) {
    if (written + task.iovs[i].iov_len > total_progress) {
      auto offset_in_block = total_progress - written;

      // set first buffer to correct offset
      buff_iovecs[0].iov_base = &reinterpret_cast<uint8_t *>(task.iovs[i].iov_base)[offset_in_block];
      buff_iovecs[0].iov_len = task.iovs[i].iov_len - offset_in_block;
      // set remaining buffers to rest of the iovecs
      for (size_t j = 1; j + i < task.num_iovecs; j++) {
        buff_iovecs[j].iov_base = task.iovs[i + j].iov_base;
        buff_iovecs[j].iov_len = task.iovs[i + j].iov_len;
      }
      break;
    } else if (written + task.iovs[i].iov_len == total_progress) {
      // set remaining buffers to rest of the iovecs
      // i++ because this entire iovec has been written, so begin writing from next one
      i++;
      for (int j = 0; j + i < task.num_iovecs; j++) {
        buff_iovecs[j].iov_base = task.iovs[i + j].iov_base;
        buff_iovecs[j].iov_len = task.iovs[i + j].iov_len;
      }
      break;
    }
    written += task.iovs[i].iov_len;
  }

  task.progress += write_metadata.op_res_num;

  ev->submit_writev(pfd, buff_iovecs, task.num_iovecs, task_id);
}

void network_server::readv_callback(processed_data_vecs read_metadata, uint64_t pfd, uint64_t task_id) {
  auto &task = task_data[task_id];

  if (read_metadata.op_res_num < 0) {
    switch (errno) {
    case EINTR:
      // for these errors, just try again, otherwise fail
      ev->submit_readv(pfd, read_metadata.iovs, read_metadata.num_vecs, task_id);
      break;
    default: {
      // in case of any of these errors, just close the socket
      ev->shutdown_and_close_normally(pfd);
      // there was some other error
      application_close_callback(pfd, task_id);
      return;
    }
    }
  }

  // most this code is nearly identical to stuff for writev, consider refactoring
  // into a single helper method later

  auto total_progress = task.progress + read_metadata.op_res_num;

  auto buff_iovecs = reinterpret_cast<iovec *>(task.buff);
  size_t read_so_far{};

  if (total_progress >= task.buff_length) {
    callbacks->raw_readv_callback(task.iovs, task.num_iovecs, pfd);
    close_pfd_gracefully(pfd, task_id);
    FREE(task.iovs); // writev allocates some memory
    return;
  }

  // so all buffer offsets/lengths are reset
  std::memset(buff_iovecs, 0, sizeof(iovec) * task.num_iovecs);

  for (size_t i = 0; i < task.num_iovecs; i++) {
    if (read_so_far + task.iovs[i].iov_len > total_progress) {
      auto offset_in_block = total_progress - read_so_far;

      // set first buffer to correct offset
      buff_iovecs[0].iov_base = &reinterpret_cast<uint8_t *>(task.iovs[i].iov_base)[offset_in_block];
      buff_iovecs[0].iov_len = task.iovs[i].iov_len - offset_in_block;
      // set remaining buffers to rest of the iovecs
      for (size_t j = 1; j + i < task.num_iovecs; j++) {
        buff_iovecs[j].iov_base = task.iovs[i + j].iov_base;
        buff_iovecs[j].iov_len = task.iovs[i + j].iov_len;
      }
      break;
    } else if (read_so_far + task.iovs[i].iov_len == total_progress) {
      // set remaining buffers to rest of the iovecs
      // i++ because this entire iovec has been written, so begin writing from next one
      i++;
      for (int j = 0; j + i < task.num_iovecs; j++) {
        buff_iovecs[j].iov_base = task.iovs[i + j].iov_base;
        buff_iovecs[j].iov_len = task.iovs[i + j].iov_len;
      }
      break;
    }
    read_so_far += task.iovs[i].iov_len;
  }

  task.progress += read_metadata.op_res_num;

  ev->submit_readv(pfd, buff_iovecs, task.num_iovecs, task_id);
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

    free_task(task_id); // done writing, task can be freed now
  }
}

void network_server::shutdown_callback(int how, uint64_t pfd, int op_res_num, uint64_t task_id) {
  if (op_res_num < 0) {
    // in case of any of these errors, just close the socket
    ev->shutdown_and_close_normally(pfd);
    // there was some other error
    application_close_callback(pfd, task_id);
  }

  // only will be submitting shutdown using SHUT_RDWR
  auto &task = task_data[task_id];

  if (task.last_read_zero) {
    ev->close_pfd(pfd, task_id);
  } else {
    task.shutdown = true;
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
      ev->submit_generic_event(pfd, additional_info);
      break;
    default: {
      // in case of any of these errors, just close the socket
      ev->shutdown_and_close_normally(pfd);

      // there was some other error
      callbacks->event_error_close_callback(pfd, additional_info);
      return;
    }
    }
  }

  callbacks->event_trigger_callback(pfd, additional_info);
}