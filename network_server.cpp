#include "network_server.hpp"
#include "metadata.hpp"

network_server::network_server(int port, application_methods *callbacks) {
  if (callbacks == nullptr) {
    std::string error = "Application methods callbacks must be set (" + std::string(__FUNCTION__);
    error += ": " + std::to_string(__LINE__);
    utility::fatal_error(error);
  }

  ev->set_server_methods(this);

  this->callbacks = callbacks;

  listener_pfd = utility::setup_listener_pfd(port, ev);
  ev->submit_accept(listener_pfd);

  ev->start();
}

int network_server::get_task() {
  int id = 0;

  if(task_freed_idxs.size() > 0) {
    id = *task_freed_idxs.begin();
    task_freed_idxs.erase(id);

    auto &freed_task = task_data[id];
    freed_task = task();
  } else {
    task_data.emplace_back();
  }
}

int network_server::get_task(uint8_t *buff, size_t length) {
  auto id = get_task();
  auto &task = task_data[id];
  task.buff = buff;
  task.buff_length = length;

  return id;
}

void network_server::free_task(int id) {
  task_freed_idxs.insert(id);
}

void network_server::accept_callback(int listener_fd, sockaddr_storage *user_data,
                                     socklen_t size, uint64_t pfd, int op_res_num, uint64_t task_id) {
  if (op_res_num < 0) {
    switch (errno) {
    // for these errors, just try again, otherwise fail
    case ECONNABORTED:
    case EINTR:
      ev->submit_accept(pfd);
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

  // also read from the user
  auto user_task_id = get_task();
  auto &task = task_data[user_task_id];

  auto buff = new uint8_t[READ_SIZE];
  task.buff = buff;
  task.buff_length = READ_SIZE;
  task.op_type = operation_type::NETWORK_READ;

  // queues up the read, passes the task id as additional info
  ev->queue_read(pfd, task.buff, task.buff_length, user_task_id);

  // carry on listening, submits everything in the queue with it, not using task_id for this
  ev->submit_accept(listener_fd);
}

void network_server::read_callback(processed_data read_metadata, uint64_t pfd, uint64_t task_id) {
  if (read_metadata.op_res_num < 0) {
    switch (errno) {
    // for these errors, just try again, otherwise fail
    case EINTR:
      ev->submit_read(pfd, read_metadata.buff, read_metadata.length);
      break;
    default: {
      // in case of any of these errors, just close the socket
      ev->shutdown_and_close_normally(pfd);

      // there was some other error
      callbacks->close_callback(pfd);
    }
    }

    return;
  }

  if (read_metadata.op_res_num == 0) { // read was 0, socket connection closed
    auto shutdown_code = ev->submit_shutdown(pfd, SHUT_WR);

    if (shutdown_code < 0) {                // shutdown procedure failed
      ev->shutdown_and_close_normally(pfd); // use normal shutdown()/close()
      this->close_callback(pfd, 0);           // clean up resources in network server/application
    }

    return;
  }

  auto &task = task_data[task_id];
  read_data data{};

  // network reads will read once up to READ_SIZE bytes, whereas application reads read as much as they can
  // before returning what was requested
  if(task.op_type == operation_type::APPLICATION_READ) {
    size_t read_this_time = read_metadata.op_res_num;
    auto total_progress = read_this_time + read_metadata.op_res_num;

    if(total_progress < task.buff_length) {
      ev->submit_read(pfd, &task.buff[total_progress], task.buff_length - total_progress);
      task.progress = total_progress;
      return;
    } else { // finished reading
      data.buffer = read_metadata.buff;
      data.read_amount = total_progress;
    }

  } else if(task.op_type == operation_type::NETWORK_READ) {
    size_t read_this_time = read_metadata.op_res_num; // dealt with case that this is < 0 now

    // populate the data struct with necessary and useful info
    data.buffer = read_metadata.buff;
    data.read_amount = read_this_time;
  }
  
  callbacks->read_callback(data, pfd);
}

void network_server::write_callback(processed_data write_metadata, uint64_t pfd, uint64_t task_id) {
  if (write_metadata.op_res_num < 0) {
    switch (errno) {
    // for these errors, just try again, otherwise fail
    case EINTR:
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

  // write code
}

void network_server::shutdown_callback(int how, uint64_t pfd, int op_res_num, uint64_t task_id) {
  if (op_res_num < 0) {
    // in case of any of these errors, just close the socket
    ev->shutdown_and_close_normally(pfd);

    // there was some other error
    callbacks->close_callback(pfd);
    return;
  }

  // shutdown code
}

void network_server::close_callback(uint64_t pfd, int op_res_num, uint64_t task_id) {
  if (op_res_num < 0) {
    // in case of any of these errors, just close the socket
    ev->shutdown_and_close_normally(pfd);

    // there was some other error
    callbacks->close_callback(pfd);
    return;
  }
}

void network_server::event_callback(int pfd, int op_res_num, uint64_t task_id) {
  if (op_res_num < 0) {
    switch (errno) {
    // for these errors, just try again, otherwise fail
    case EINTR:
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

  // event code
}