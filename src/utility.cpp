#include <cstring>
#include <netdb.h>
#include <netinet/tcp.h>

#include "header/utility.hpp"

namespace utility {

struct b64quartet {
  b64quartet(char a, char b, char c) {
    static char b64_alphabet[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
                                  'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
                                  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
                                  'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
                                  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};
    str[0] = b64_alphabet[(a & 0b11111100) >> 2];
    str[1] = b64_alphabet[(a & 0b00000011) << 4 | (b & 0b11110000) >> 4];
    str[2] = b64_alphabet[(b & 0b00001111) << 2 | (c & 0b11000000) >> 6];
    str[3] = b64_alphabet[(c & 0b00111111)];
    str[4] = 0;
  }
  char str[5];
};

std::string b64_encode(const char *str) {
  int len = strlen(str);

  std::string b64_str{};

  for (int i = 0; i < len; i += 3) {
    char a = str[i];
    char b = i + 1 < len ? str[i + 1] : 0;
    char c = i + 2 < len ? str[i + 2] : 0;

    b64_str += b64quartet(a, b, c).str;
  }

  int b64_str_len = b64_str.size();
  int num_padding = (3 - len % 3) % 3;
  for (int i = b64_str_len - num_padding; i < b64_str_len; i++) {
    b64_str[i] = '=';
  }

  return b64_str;
}

void log_helper_function(std::string msg, bool cerr_or_not) {
#ifdef VERBOSE_DEBUG
  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  std::string time = asctime(tm);
  time.pop_back();
  std::cout << "[ " << time << " ]: " << msg << std::endl;
#endif
}

void fatal_error(std::string error_message) {
  perror(std::string("Fatal Error: " + error_message).c_str());
  exit(1);
}

int setup_listener_pfd(int port, event_manager *ev) {
  int listener_fd{};

  int yes = true;
  addrinfo hints, *server_info, *traverser;

  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;       // IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM; // tcp
  hints.ai_flags = AI_PASSIVE;     // use local IP

  int ret_addrinfo{}, ret_bind{};
  ret_addrinfo = getaddrinfo(NULL, std::to_string(port).c_str(), &hints, &server_info);

  for (traverser = server_info; traverser != NULL; traverser = traverser->ai_next) {
    // make the pfd
    listener_fd = socket(traverser->ai_family, traverser->ai_socktype, traverser->ai_protocol);

    if (listener_fd < 0) {
      utility::fatal_error("Opening socket for listening failed");
    }

    // set socket flags
    setsockopt(listener_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    setsockopt(listener_fd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes));
    setsockopt(listener_fd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes));

    int keep_idle = 100; // The time (in seconds) the connection needs to remain idle before
                         // TCP starts sending keepalive probes, if the socket option
                         // SO_KEEPALIVE has been set on this socket.  This option should
                         // not be used in code intended to be portable.
    setsockopt(listener_fd, IPPROTO_TCP, TCP_KEEPIDLE, &keep_idle, sizeof(keep_idle));
    int keep_interval = 100; // The time (in seconds) between individual keepalive probes. This
                             // option should not be used in code intended to be portable.
    setsockopt(listener_fd, IPPROTO_TCP, TCP_KEEPINTVL, &keep_interval, sizeof(keep_interval));
    int keep_count = 5; // The maximum number of keepalive probes TCP should
                        // send before dropping the connection.  This option
                        // should not be used in code intended to be portable.
    setsockopt(listener_fd, IPPROTO_TCP, TCP_KEEPCNT, &keep_count, sizeof(keep_count));
    ret_bind = bind(listener_fd, traverser->ai_addr, traverser->ai_addrlen);
  }

  if (ret_addrinfo < 0 || ret_bind < 0) {
    std::cerr << "addrinfo or bind failed: " << ret_addrinfo << " # " << ret_bind << "\n";
    return -1;
  }

  freeaddrinfo(server_info);

  if (listen(listener_fd, LISTEN_BACKLOG) < 0) {
    return -1;
  }

  return ev->pass_fd_to_event_manager(listener_fd, true);
}

} // namespace utility