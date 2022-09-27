#ifndef NETWORK_SERVER_EXAMPLE
#define NETWORK_SERVER_EXAMPLE

constexpr int port = 4000;

#include "../header/http_response.h"
#include "../network_server.hpp"

class app_methods : public application_methods {
public:
  void http_read_callback(http_request req, int client_num, bool failed_req) override;
  void http_write_callback(buff_data data, int client_num, bool failed_req) override;
  void http_writev_callback(struct iovec *data, size_t num_iovecs, int client_num, bool failed_req) override;
  void http_close_callback(int client_num) override;

  app_methods() {
    event_manager ev{};
    network_server ns{port, &ev, this};

    this->set_network_server(&ns);

    ns.start();
  }
};

#endif