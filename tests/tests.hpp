#ifndef NETWORK_TESTS_HPP
#define NETWORK_TESTS_HPP

#include "header/debug_mem_ops.hpp"
#include "network_server.hpp"

inline char *malloc_str(std::string str) {
  char *buff = (char*)MALLOC(str.size()+1);
  memcpy(buff, str.c_str(), str.size());
  return buff;
}

class app : public application_methods {
public:
  void http_read_callback(http_request &&req, int client_num, bool failed_req) override {}
  void http_write_callback(buff_data data, int client_num, bool failed_req) override {}
  void http_writev_callback(struct iovec *data, size_t num_iovecs, int client_num, bool failed_req) override {}
  void http_close_callback(int client_num) override {}
};

#endif