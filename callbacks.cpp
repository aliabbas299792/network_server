#include "metadata.hpp"
#include "network_server.hpp"
#include "subprojects/event_manager/header/event_manager_metadata.hpp"
#include <cstring>

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

  auto buff = new uint8_t[READ_SIZE];
  task.buff = buff;
  task.buff_length = READ_SIZE;
  task.op_type = operation_type::NETWORK_UNKNOWN; // don't know what it is yet

  // queues up the read, passes the task id as additional info
  if (ev->queue_read(pfd, task.buff, task.buff_length, user_task_id) < 0) {
    // if the queueing operation didn't work, end this connection
    free(buff);
    free_task(user_task_id);
    ev->shutdown_and_close_normally(pfd);
  }

  // carry on listening, submits everything in the queue with it, not using task_id for this
  if (ev->submit_accept(listener_fd)) {
    utility::fatal_error("Submit accept normal resubmit failed");
  }
}

void network_server::read_callback(processed_data read_metadata, uint64_t pfd, uint64_t task_id) {
  auto &task = task_data[task_id];

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
    // if shutdown not done, or if shutdown done but close_pfd failed
    // then cleanup
    if (!task.shutdown || ev->close_pfd(pfd) < 0) {
      // shouldn't get here usually, but this is in case it does
      ev->shutdown_and_close_normally(pfd);
      application_close_callback(pfd, task_id);
    }
    free_task(task_id);
    return;
  }

  buff_data data{};

  // network reads will read once up to READ_SIZE bytes, whereas application reads read as much as they can
  // before returning what was requested
  switch (task.op_type) {
  case operation_type::NETWORK_UNKNOWN:
    initialisation_accept_procedure(pfd, task_id);

    // initialisation should make it into either RAW_READ, WEBSOCKET_READ or HTTP_READ
    if (task.op_type != RAW_READ) {
      if (ev->submit_read(pfd, task.buff, READ_SIZE, task_id) < 0) {

        ev->shutdown_and_close_normally(pfd);
        application_close_callback(pfd, task_id);
        free_task(task_id);
      }
    }
    return; // return here since we don't need to call any callbacks from here
  case operation_type::RAW_READ: {
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
  case operation_type::HTTP_READ:
  case operation_type::WEBSOCKET_READ: {
    size_t read_this_time = read_metadata.op_res_num; // dealt with case that this is < 0 now
    // populate the data struct with necessary and useful info
    data.buffer = read_metadata.buff;
    data.size = read_this_time;

    task.progress = 0;
    task.shutdown = false;
    std::memset(task.buff, 0, READ_SIZE); // reuse task since we're just going to read READ_SIZE bytes again

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

  switch (task.op_type) {
  case HTTP_READ:
    callbacks->http_read_callback(data, pfd);
    break;
  case RAW_READ:
    callbacks->raw_read_callback(data, pfd);
    break;
  case WEBSOCKET_READ:
    callbacks->websocket_read_callback(data, pfd);
    break;
  default:
    // shouldn't get here usually, but this is in case it does
    ev->shutdown_and_close_normally(pfd);
    application_close_callback(pfd, task_id);
    free_task(task_id);
    break;
  }
}

void network_server::write_callback(processed_data write_metadata, uint64_t pfd, uint64_t task_id) {
  auto &task = task_data[task_id];

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
      application_close_callback(pfd, task_id);
      return;
    }
    }
  }

  auto total_progress = task.progress + write_metadata.op_res_num;

  if (total_progress < task.buff_length) {
    ev->submit_write(pfd, &task.buff[total_progress], task.buff_length - total_progress, task_id);
    task.progress = total_progress;
  } else {
    buff_data data{total_progress, task.buff};

    switch (task.op_type) {
    case HTTP_WRITE:
      callbacks->http_write_callback(data, pfd);
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
  task_data[task_id].shutdown = true;
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