#include "tests.hpp"
#include "network_server.hpp"
#include <chrono>
#include <thread>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "vendor/doctest/doctest/doctest.h"

class app : public application_methods {
public:
  void http_read_callback(http_request &&req, int client_num, bool failed_req) override {}
  void http_write_callback(buff_data data, int client_num, bool failed_req) override {}
  void http_writev_callback(struct iovec *data, size_t num_iovecs, int client_num, bool failed_req) override {}
  void http_close_callback(int client_num) override {}
};

TEST_CASE("network server full tests") {
  event_manager ev{};
  app app{};
  network_server ns{4001, &ev, &app};

  auto t1 = std::thread([&] {
    ns.start();
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  ns.stop();

  t1.join();

}
