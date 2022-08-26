#include "network_server.hpp"
#include <cstring>

void network_server::accept_callback(int listener_fd, sockaddr_storage *user_data, socklen_t size,
                                     uint64_t pfd, int op_res_num, uint64_t task_id) {
  if (op_res_num < 0) {
    switch (errno) {
    // for these errors, just try again, otherwise fail
    case ECONNABORTED:
    case EINTR:
      ev->submit_accept(pfd, task_id);
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

  callbacks->accept_callback(pfd);

  // for any accept task, a read request is submitted and the accept request is resubmitted
  // if the user want this to be cancelled, they can just call close using the client num

  // also read from the user
  auto user_task_id = get_task();
  auto &task = task_data[user_task_id];

  auto buff = new uint8_t[READ_SIZE];
  task.buff = buff;
  task.buff_length = READ_SIZE;
  task.op_type = operation_type::NONE; // don't know what it is yet

  // queues up the read, passes the task id as additional info
  ev->queue_read(pfd, task.buff, task.buff_length, user_task_id);

  // carry on listening, submits everything in the queue with it, not using task_id for this
  ev->submit_accept(listener_fd);

  callbacks->accept_callback(pfd);
}

void network_server::read_callback(processed_data read_metadata, uint64_t pfd, uint64_t task_id) {
  if (read_metadata.op_res_num < 0) {
    switch (errno) {
    // for these errors, just try again, otherwise fail
    case EINTR:
      ev->submit_read(pfd, read_metadata.buff, read_metadata.length, task_id);
      break;
    default: {
      // in case of any of these errors, just close the socket
      ev->shutdown_and_close_normally(pfd);

      // there was some other error
      close_procedure(pfd);

      free_task(task_id);
    }
    }

    return;
  }

  if (read_metadata.op_res_num == 0) { // read was 0, socket connection closed
    auto &task = task_data[task_id];

    switch (task.op_type) {
    case operation_type::HTTP_READ:
      task.op_type = operation_type::HTTP_CLOSE;
      break;
    case operation_type::WEBSOCKET_READ:
      task.op_type = operation_type::WEBSOCKET_CLOSE;
      break;
    case operation_type::RAW_READ:
      task.op_type = operation_type::RAW_CLOSE;
      break;
    default:
      task.op_type = operation_type::NONE;
    }

    auto shutdown_code = ev->submit_shutdown(pfd, SHUT_WR, task_id);

    if (shutdown_code < 0) {                // shutdown procedure failed
      ev->shutdown_and_close_normally(pfd); // use normal shutdown()/close()
      this->close_callback(pfd, 0);         // clean up resources in network server/application
    }

    return;
  }

  auto &task = task_data[task_id];
  buff_data data{};

  // network reads will read once up to READ_SIZE bytes, whereas application reads read as much as they can
  // before returning what was requested
  if (task.op_type == operation_type::APPLICATION_READ) {
    auto total_progress = task.progress + read_metadata.op_res_num;

    if (total_progress < task.buff_length) {
      ev->submit_read(pfd, &task.buff[total_progress], task.buff_length - total_progress, task_id);
      task.progress = total_progress;
      return;
    } else { // finished reading
      data.buffer = read_metadata.buff;
      data.read_amount = total_progress;

      free_task(task_id); // task is completed so task id is freed (we assume user will free the buffer)
    }

  } else if (task.op_type == operation_type::NETWORK_READ) {
    size_t read_this_time = read_metadata.op_res_num; // dealt with case that this is < 0 now

    // populate the data struct with necessary and useful info
    data.buffer = read_metadata.buff;
    data.read_amount = read_this_time;

    std::memset(task.buff, 0, READ_SIZE); // reuse task since we're just going to read READ_SIZE bytes again
    ev->submit_read(pfd, task.buff, READ_SIZE, task_id);

    // don't need to free the task here, we're just reusing the older one
  }

  callbacks->read_callback(data, pfd);
}

void network_server::write_callback(processed_data write_metadata, uint64_t pfd, uint64_t task_id) {
  if (write_metadata.op_res_num < 0) {
    switch (errno) {
    case EINTR:
      // for these errors, just try again, otherwise fail
      ev->submit_read(pfd, write_metadata.buff, write_metadata.length, task_id);
      break;
    default: {
      // in case of any of these errors, just close the socket
      ev->shutdown_and_close_normally(pfd);

      // there was some other error
      callbacks->close_callback(pfd);

      free_task(task_id);
      return;
    }
    }
  }

  auto &task = task_data[task_id];

  auto total_progress = task.progress + write_metadata.op_res_num;

  if (total_progress < task.buff_length) {
    ev->submit_write(pfd, &task.buff[total_progress], task.buff_length - total_progress, task_id);
    task.progress = total_progress;
  } else {
    buff_data data{total_progress, task.buff};
    callbacks->write_callback(data, pfd);

    free_task(task_id); // done writing, task can be freed now
  }
}

void network_server::shutdown_callback(int how, uint64_t pfd, int op_res_num, uint64_t additional_info) {

  if (op_res_num < 0) {
    // in case of any of these errors, just close the socket
    ev->shutdown_and_close_normally(pfd);

    // there was some other error
    callbacks->close_callback(pfd);
  }

  switch (how) {
  case SHUT_RD:
    break;
  case SHUT_RDWR:
  case SHUT_WR:
    ev->submit_close(pfd);
  }
}

void network_server::close_callback(uint64_t pfd, int op_res_num, uint64_t additional_info) {
  if (op_res_num < 0) {
    // in case of any of these errors, just close the socket
    ev->shutdown_and_close_normally(pfd);
  }

  callbacks->close_callback(pfd);
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
      callbacks->close_callback(pfd);
      return;
    }
    }
  }

  callbacks->event_callback(additional_info, pfd);
}