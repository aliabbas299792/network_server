#include "header/debug_mem_ops.hpp"
#include "header/utility.hpp"
#include "network_server.hpp"
#include <cstdint>
#include <linux/limits.h>
#include <signal.h>
#include <sys/inotify.h>

network_server::network_server(int port, event_manager *ev, application_methods *callbacks) {
  // signal handler for when a connection is closed while writing
  signal(SIGPIPE, SIG_IGN);

  // callbacks used by the network server
  if (callbacks == nullptr) {
    FATAL_ERROR("Application methods callbacks must be set (" << __FUNCTION__ << ": " << __LINE__);
  }
  this->callbacks = callbacks;

  // event manager setup
  this->ev = ev;
  ev->set_callbacks(this);

  // initialise listening
  listener_pfd = utility::setup_listener_pfd(port, ev);
  if (ev->submit_accept(listener_pfd) < 0) {
    FATAL_ERROR("Listener accept failed");
  }

  // initial read for the cache
  int inotify_fd = cache.get_inotify_fd();
  int inotify_pfd = ev->pass_fd_to_event_manager(inotify_fd);
  size_t size_buff = sizeof(inotify_event) + NAME_MAX + 1;
  uint8_t *inotify_read_buff = (uint8_t*)MALLOC(size_buff);
  int inotify_read_task = get_task_buff_op(INOTIFY_READ, inotify_read_buff, size_buff);
  ev->submit_read(inotify_pfd, inotify_read_buff, size_buff, inotify_read_task);
}

void network_server::start() { ev->start(); }

void network_server::stop() { ev->kill(); }

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

int network_server::get_task_buff_op(operation_type type, uint8_t *buff, size_t length) {
  auto id = get_task();
  auto &task = task_data[id];
  task.op_type = type;

  if (buff != nullptr) {
    task.buff = buff;
    task.buff_length = length;
  }

  return id;
}

int network_server::get_task_vector_op(operation_type type, struct iovec *iovecs, size_t num_iovecs) {
  auto id = get_task();
  auto &task = task_data[id];
  task.op_type = type;

  if (iovecs != nullptr) {
    task.buff = reinterpret_cast<uint8_t *>(iovecs);
    task.num_iovecs = num_iovecs;

    // will store the original iovecs so we can free them later
    // the buffer stuff is set using this as reference
    task.iovs = (struct iovec *)MALLOC(num_iovecs * sizeof(iovec));
    memcpy(task.iovs, task.buff, num_iovecs * sizeof(iovec));
  }

  return id;
}

void network_server::free_task(int task_id) {
  PRINT_DEBUG("freeing task id: " << task_id << ", container size: " << task_freed_idxs.size());
  PRINT_DEBUG(" \t\t--- not actually freeing it ---\n");
  // task_data[task_id] = {};
  // task_freed_idxs.insert(task_id);
}

void network_server::application_close_callback(int pfd, uint64_t task_id) {
  if ((int64_t)task_id < 0)
    return;

  const auto &task = task_data[task_id];

  switch (task.op_type) {
    break;
  case NETWORK_READ:
  case HTTP_WRITE:
  case HTTP_WRITEV:
  case HTTP_CLOSE:
  case HTTP_SEND_FILE:
  case HTTP_POST_READ:
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
  case INOTIFY_READ:
  case EVENT_READ: // don't expect to deal with this here
    break;
  }

  if (task_id != -1u)
    free_task(task_id);
  // task is no longer needed since related pfd has been closed
}

void network_server::network_read_procedure(int pfd, uint64_t task_id, bool *should_auto_resubmit_read, bool failed_req, buff_data data) {
  // http_response returns true if it was valid data and took action
  // websocket_frame_response returns true if it's a websocket frame
  // otherwise close the connection
  if (!http_response_method(pfd, should_auto_resubmit_read, data, failed_req) &&
      !websocket_frame_response_method(pfd, should_auto_resubmit_read, data, failed_req) && !failed_req) {
    // only need to call close method if it isn't a failed request, otherwise it is already going to close
    ev->close_pfd(pfd, task_id);
  }
}

int network_server::get_client_num_from_fd(int fd) {
  return ev->pass_fd_to_event_manager(fd);
}