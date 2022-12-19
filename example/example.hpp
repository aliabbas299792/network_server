#ifndef NETWORK_SERVER_EXAMPLE
#define NETWORK_SERVER_EXAMPLE

constexpr int port = 4001;

#include "header/http_response.hpp"
#include "network_server.hpp"

struct job_data {
  int client_num = -1;
  std::string filepath{};
};

class app_methods : public application_methods {
public:
  void raw_read_callback(buff_data data, int client_num, bool failed_req = false) override;

  void http_read_callback(http_request &&req, int client_num, bool failed_req) override;
  void http_write_callback(buff_data data, int client_num, bool failed_req) override;
  void http_writev_callback(struct iovec *data, size_t num_iovecs, int client_num, bool failed_req) override;
  void http_close_callback(int client_num) override;

  std::string base_file_path{};

  std::vector<job_data> job_requests{};

  app_methods(const std::string &base_file_path) {
    event_manager ev{};
    network_server ns{port, &ev, this};

    this->base_file_path = base_file_path;
    this->set_network_server(&ns);

    ns.start();
  }
};

#endif