#include "network_server.hpp"

// same as normal read but carries info about what connection type
int network_server::raw_read(int pfd, buff_data data) {
  auto task_id = get_task();
  auto &task = task_data[task_id];
  task.op_type = operation_type::RAW_WRITE;

  return ev->submit_read(pfd, data.buffer, data.size);
}

int network_server::raw_write(int pfd, buff_data data) {
  auto task_id = get_task();
  auto &task = task_data[task_id];
  task.op_type = operation_type::RAW_WRITE;

  return ev->submit_write(pfd, data.buffer, data.size);
}

int network_server::raw_close(int pfd) {
  auto task_id = get_task();
  auto &task = task_data[task_id];
  task.op_type = operation_type::RAW_CLOSE;

  close_pfd_gracefully(pfd, task_id);
  return 0;
}

int network_server::eventfd_open() { return ev->create_event_fd_normally(); }

int network_server::eventfd_trigger(int pfd) { return ev->event_alert_normally(pfd); }

int network_server::eventfd_prepare(int pfd, uint64_t additional_info) {
  return ev->submit_generic_event(pfd, additional_info);
}

int network_server::local_open(const char *pathname, int flags) {
  return ev->open_get_pfd_normally(pathname, flags);
}
int network_server::local_stat(const char *pathname, struct stat *buff) {
  return ev->stat_normally(pathname, buff);
}
int network_server::local_fstat(int pfd, struct stat *buff) { return ev->fstat_normally(pfd, buff); }
int network_server::local_unlink(const char *pathname) { return ev->unlink_normally(pathname); }