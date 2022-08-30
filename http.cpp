#include "http_request.h"
#include "http_response.h"
#include "network_server.hpp"

bool network_server::http_response_method(int pfd, buff_data data) {
  http_request req{reinterpret_cast<char *>(data.buffer)};

  if (req.valid_req) {
    callbacks->http_read_callback(req, pfd);
    return true;
  }

  return false;
}

// read not reading since this is auto submitted
int network_server::http_write(int pfd, char *buff, size_t buff_length) {
  auto task_id = get_task(operation_type::HTTP_WRITE);

  return ev->submit_write(pfd, reinterpret_cast<uint8_t *>(buff), buff_length, task_id);
}

int network_server::http_close(int pfd) {
  auto task_id = get_task();
  auto &task = task_data[task_id];
  task.op_type = operation_type::HTTP_CLOSE;

  close_pfd_gracefully(pfd, task_id);
  return 0;
}