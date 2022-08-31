#include "network_server.hpp"

bool network_server::websocket_frame_response_method(int pfd, buff_data data) { return false; }

template <int_range T> int websocket_broadcast(const T &container, buff_data data) {
  for (const auto pfd : container) {
    // ev->queue_write(pfd, uint8_t *buffer, size_t length)
  }
  return 1;
}

int network_server::websocket_write(int pfd, buff_data data) {
  auto task_id = get_task(operation_type::WEBSOCKET_WRITE, data.buffer, data.size);

  // make a new buffer which is slightly bigger
  // add in websocket headings and stuff in there

  return ev->submit_write(pfd, data.buffer, data.size, task_id);
}

int network_server::websocket_close(int pfd) {
  auto task_id = get_task();
  auto &task = task_data[task_id];
  task.op_type = operation_type::WEBSOCKET_CLOSE;

  close_pfd_gracefully(pfd, task_id);
  return 0;
}