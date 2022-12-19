#ifndef NETWORK_TESTS_HPP
#define NETWORK_TESTS_HPP

#include "network_server.hpp"

class app : public application_methods {
public:
  void http_read_callback(http_request &&req, int client_num, bool failed_req) override {}
  void http_write_callback(buff_data data, int client_num, bool failed_req) override {}
  void http_writev_callback(struct iovec *data, size_t num_iovecs, int client_num, bool failed_req) override {}
  void http_close_callback(int client_num) override {}
};

#endif