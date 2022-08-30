#include "http_request.h"
#include "http_response.h"
#include "network_server.hpp"
#include "subprojects/event_manager/event_manager.hpp"

constexpr int port = 4000;

class app_methods : public application_methods {
public:
  void http_read_callback(http_request req, int client_num) override {
    std::string content = "<h1>Hello world!</h1>";
    http_response resp{http_resp_codes::RESP_200_OK, http_ver::HTTP_10, false, "text/html", content};
    auto resp_data = resp.allocate_buffer();
    ns->http_write(client_num, resp_data, resp.length());
  }

  app_methods() {
    event_manager ev{};
    network_server ns{port, &ev, this};

    this->set_network_server(&ns);

    ns.start();
  }
};

int main() { app_methods{}; }
