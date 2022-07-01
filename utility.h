#include <string>

namespace utility {

void fatal_error(std::string error_message);
void log_helper_function(std::string msg, bool cerr_or_not);
std::string b64_encode(const char *str);

}; // namespace utility