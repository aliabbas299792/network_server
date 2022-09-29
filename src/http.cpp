#include "../header/http_response.h"
#include "network_server.hpp"

bool network_server::http_response_method(int pfd, buff_data data, bool failed_req) {
  http_request req{reinterpret_cast<char *>(data.buffer)};

  if (req.valid_req) {
    callbacks->http_read_callback(std::move(req), pfd, failed_req);
    return true;
  }

  return false;
}

int network_server::http_writev(int pfd, struct iovec *iovs, size_t num_iovecs) {
  auto task_id = get_task(operation_type::HTTP_WRITEV, iovs, num_iovecs);
  return ev->submit_writev(pfd, iovs, num_iovecs, task_id);
}

// read not reading since this is auto submitted
int network_server::http_write(int pfd, char *buff, size_t buff_length) {
  auto buff_ptr = reinterpret_cast<uint8_t *>(buff);
  auto task_id = get_task(operation_type::HTTP_WRITE, buff_ptr, buff_length);
  return ev->submit_write(pfd, buff_ptr, buff_length, task_id);
}

int network_server::http_close(int pfd) {
  auto task_id = get_task();
  auto &task = task_data[task_id];
  task.op_type = operation_type::HTTP_CLOSE;

  close_pfd_gracefully(pfd, task_id);
  return 0;
}