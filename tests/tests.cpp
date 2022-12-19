#include "tests.hpp"
#include "network_server.hpp"
#include <chrono>
#include <thread>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "vendor/doctest/doctest/doctest.h"

TEST_CASE("network server full tests") {
  SUBCASE("Starts and stops correctly") {
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
}
