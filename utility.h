#include <iostream>

namespace utility {
inline void fatal_error(std::string error_message) {
  perror(std::string("Fatal Error: " + error_message).c_str());
  exit(1);
}

inline void log_helper_function(std::string msg, bool cerr_or_not) {
  std::cout << "[ " << __DATE__ << " | " << __TIME__ << " ]: " << msg
            << std::endl;
}
}; // namespace utility