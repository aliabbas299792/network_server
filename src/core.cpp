#include "../header/debug_mem_ops.hpp"
#include "network_server.hpp"
#include <signal.h>

network_server::network_server(int port, event_manager *ev, application_methods *callbacks) {
  signal(SIGPIPE, SIG_IGN); // signal handler for when a connection is closed while writing

  if (callbacks == nullptr) {
    std::string error = "Application methods callbacks must be set (" + std::string(__FUNCTION__);
    error += ": " + std::to_string(__LINE__);
    utility::fatal_error(error);
  }

  this->ev = ev;
  ev->set_server_methods(this);

  this->callbacks = callbacks;

  listener_pfd = utility::setup_listener_pfd(port, ev);
  if (ev->submit_accept(listener_pfd) < 0) {
    utility::fatal_error("Listener accept failed");
  }
}

void network_server::start() { ev->start(); }

int network_server::get_task() {
  int id = 0;

  if (task_freed_idxs.size() > 0) {
    id = *task_freed_idxs.begin();
    task_freed_idxs.erase(id);

    auto &freed_task = task_data[id];
    freed_task = task();
  } else {
    task_data.emplace_back();
    id = task_data.size() - 1;
  }

  return id;
}

int network_server::get_task(operation_type type, uint8_t *buff, size_t length) {
  auto id = get_task();
  auto &task = task_data[id];
  task.op_type = type;

  if (buff != nullptr) {
    task.buff = buff;
    task.buff_length = length;
  }

  return id;
}

int network_server::get_task(operation_type type, struct iovec *iovecs, size_t num_iovecs) {
  auto id = get_task();
  auto &task = task_data[id];
  task.op_type = type;

  if (iovecs != nullptr) {
    task.buff = reinterpret_cast<uint8_t *>(iovecs);
    task.num_iovecs = num_iovecs;

    task.buff_length = 0;
    for (size_t i = 0; i < num_iovecs; i++) {
      task.buff_length += iovecs[i].iov_len;
    }

    // will store the original iovecs so we can free them later
    // the buffer stuff is set using this as reference
    task.iovs = (struct iovec *)MALLOC(num_iovecs * sizeof(iovec));
    memcpy(task.iovs, task.buff, num_iovecs * sizeof(iovec));
  }

  return id;
}

void network_server::free_task(int task_id) { task_freed_idxs.insert(task_id); }

void network_server::close_pfd_gracefully(int pfd, uint64_t task_id) {
  const auto &pfd_info = ev->get_pfd_data(pfd);
  auto &task_info = task_data[task_id];

  switch (task_info.op_type) {
  case operation_type::NETWORK_READ:
  case operation_type::HTTP_WRITE:
    task_info.op_type = operation_type::HTTP_CLOSE;
    break;
  case operation_type::WEBSOCKET_READ:
  case operation_type::WEBSOCKET_WRITE:
    task_info.op_type = operation_type::WEBSOCKET_CLOSE;
    break;
  case operation_type::RAW_READ:
  case operation_type::RAW_WRITE:
    task_info.op_type = operation_type::RAW_CLOSE;
    break;
  default:
    break;
  }

  if (pfd_info.type == fd_types::NETWORK) {
    // only HTTP_*, WEBSOCKET_*, maybe RAW_* and NETWORK_UNKNOWN
    auto shutdown_code = ev->submit_shutdown(pfd, SHUT_RDWR, task_id);

    if (shutdown_code < 0) {
      ev->shutdown_and_close_normally(pfd);
      application_close_callback(pfd, task_id);
    }
  } else {
    // only EVENT_READ and maybe RAW_*
    ev->close_pfd(pfd);
    if (task_id != -1u) {
      free_task(task_id);
    }
  }
}

void network_server::application_close_callback(int pfd, int task_id) {
  const auto &task = task_data[task_id];

  switch (task.op_type) {
    break;
  case NETWORK_READ:
  case HTTP_WRITE:
  case HTTP_WRITEV:
  case HTTP_CLOSE:
    callbacks->http_close_callback(pfd);
    break;
  case WEBSOCKET_READ:
  case WEBSOCKET_WRITE:
  case WEBSOCKET_WRITEV:
  case WEBSOCKET_CLOSE:
    callbacks->websocket_close_callback(pfd);
    break;
  case RAW_READ:
  case RAW_WRITE:
  case RAW_WRITEV:
  case RAW_CLOSE:
  case RAW_READV:
    callbacks->raw_close_callback(pfd);
    break;
  case EVENT_READ: // don't expect to deal with this here
    break;
  }

  free_task(task_id);
  // task is no longer needed since related pfd has been closed
}

void network_server::network_read_procedure(int pfd, buff_data data) {
  // http_response returns true if it was valid data and took action
  // websocket_frame_response returns true if it's a websocket frame
  // otherwise close the connection
  if (!http_response_method(pfd, data) && !websocket_frame_response_method(pfd, data)) {
    close_pfd_gracefully(pfd);
  }
}