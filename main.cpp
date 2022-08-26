#include "network_server.hpp"

constexpr int port = 4000;

int main() {
    application_methods am{};

    network_server server{port, &am};
}
