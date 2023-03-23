#include "network_server.hpp"

// same as normal read but carries info about what connection type
int network_server::raw_read(int pfd, buff_data data) {
  auto task_id = get_task_buff_op(operation_type::RAW_READ, data.buffer, data.size);
  return ev->submit_read(pfd, data.buffer, data.size, task_id);
}

int network_server::raw_readv(int pfd, struct iovec *iovs, size_t num_iovecs) {
  auto task_id = get_task_vector_op(operation_type::RAW_READV, iovs, num_iovecs);
  return ev->submit_readv(pfd, iovs, num_iovecs, task_id);
}

int network_server::raw_writev(int pfd, struct iovec *iovs, size_t num_iovecs) {
  auto task_id = get_task_vector_op(operation_type::RAW_WRITEV, iovs, num_iovecs);
  return ev->submit_readv(pfd, iovs, num_iovecs, task_id);
}

int network_server::raw_write(int pfd, buff_data data) {
  auto task_id = get_task_buff_op(operation_type::RAW_WRITE, data.buffer, data.size);
  return ev->submit_write(pfd, data.buffer, data.size, task_id);
}

int network_server::raw_close(int pfd) {
  auto task_id = get_task();
  auto &task = task_data[task_id];
  task.op_type = operation_type::RAW_CLOSE;

  ev->close_pfd(pfd, task_id);
  return 0;
}

int network_server::eventfd_open() { return ev->create_event_fd_normally(); }

int network_server::eventfd_trigger(int pfd) { return ev->event_alert_normally(pfd); }

int network_server::eventfd_prepare(int pfd, uint64_t additional_info) {
  return ev->submit_generic_event(pfd, additional_info);
}