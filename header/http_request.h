#ifndef HTTP_REQ
#define HTTP_REQ

#include <array>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

constexpr char HTTP_VER10[] = "HTTP/1.0";
constexpr char HTTP_VER11[] = "HTTP/1.1";
constexpr char ROOT_FILE[] = "index.html";

struct range {
  size_t start{};
  size_t end{};
};

struct ranges {
  range *rs{};
  size_t rs_len{};
};

class http_request {
private:
  char *range_str{};
  bool range_parse(size_t max_size, range **ranges, size_t *ranges_len) const;

  void req_type_data_parser(char *token_str);
  void http_header_parser(char *token_str);

public:
  std::string path{};
  std::string req_type{};
  std::vector<std::string> subdirs{};

  char *host{};
  char *user_agent{};
  char *accept{};
  char *accept_language{};
  char *accept_encoding{};
  char *dnt{};
  char *connection{};
  char *upgrade_insecure_requests{};
  char *upgrade{};
  char *sec_websocket_key{};
  char *x_forwarded_for{};

  ranges get_ranges(size_t max_size) const;

  bool valid_req = true; // assume it is valid from start
public:
  http_request(char *buff);

  http_request(const http_request &) = delete;
  http_request &operator=(const http_request &) = delete;

  ~http_request();
};

#endif