#include "metadata.hpp"

#include "../subprojects/event_manager/event_manager.hpp"

namespace utility {

int setup_listener_pfd(int port, event_manager *ev);

void fatal_error(std::string error_message);
void log_helper_function(std::string msg, bool cerr_or_not);
std::string b64_encode(const char *str);

}; // namespace utility