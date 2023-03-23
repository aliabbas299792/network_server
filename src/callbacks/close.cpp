#include "network_server.hpp"

void network_server::close_callback(uint64_t pfd, int op_res_num, uint64_t task_id) {
  application_close_callback(pfd, task_id);
}