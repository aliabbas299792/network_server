#include "metadata.hpp"

#include "subprojects/event_manager/event_manager.hpp"

#include <sstream>

namespace utility {

int setup_listener_pfd(int port, event_manager *ev);

void fatal_error(std::string error_message);
void log_helper_function(std::string msg, bool cerr_or_not);
std::string b64_encode(const char *str);

}; // namespace utility

// helper macros
#define FATAL_ERROR(...) utility::fatal_error(static_cast<std::ostringstream&&>(std::ostringstream{} << __VA_ARGS__).str())

#ifdef VERBOSE_DEBUG
#define PRINT_DEBUG(...) std::cout << __VA_ARGS__ << std::endl
#define PRINT_ERROR_DEBUG(...) std::cout << __VA_ARGS__ << std::endl
#define LOG_DEBUG(CERR_OR_NOT, ...) utility::log_helper_function(static_cast<std::ostringstream&&>(std::ostringstream{} << __VA_ARGS__).str(), CERR_OR_NOT)
#else
#define PRINT_DEBUG(...) ((void)0)
#define PRINT_ERROR_DEBUG(...) ((void)0)
#define LOG_DEBUG(...) ((void)0)
#endif
