#ifndef NETWORK_SERVER_UTILITY
#define NETWORK_SERVER_UTILITY

#include "metadata.hpp"
#include "vendor/cpp_utility/printing_helper.hpp"
#include "subprojects/event_manager/event_manager.hpp"

#include <sstream>

namespace utility {

int setup_listener_pfd(int port, event_manager *ev);
std::string b64_encode(const char *str);

}; // namespace utility

#endif