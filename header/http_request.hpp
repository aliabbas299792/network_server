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

struct post_content {
  const char *name{};
  const char *filename{};
  const char *content_type{};
  const char *content_disposition{};
  size_t size{};
  uint8_t *buff{};
};

class http_request {
private:
  const char CRLF[4] = "\r\n";
  const char double_CRLF[8] = "\r\n\r\n";
  size_t break_len = strlen(CRLF);
  size_t double_break_len = strlen(double_CRLF);

private:
  // POST form data boundary lines
  std::string boundary_str{};
  std::string boundary_last_str{};

  std::string range_str{};
  bool range_parse(size_t max_size, range **ranges, size_t *ranges_len, bool *valid_range) const;

  void req_type_data_parser(char *token_str);
  void http_header_parser(char *token_str);

  std::string boundary{};
  std::string boundary_last{};

public:
  std::string path{};
  std::string req_type{};
  std::vector<std::string> subdirs{};

  char *host{};
  char *user_agent{};
  char *accept{};
  char *accept_language{};
  char *accept_encoding{};
  char *content_length{};
  char *content_type{};
  char *dnt{};
  char *connection{};
  char *upgrade_insecure_requests{};
  char *upgrade{};
  char *sec_websocket_key{};
  char *x_forwarded_for{};
  char *content{};

  char *buff{};

  std::vector<range> get_ranges(size_t max_size, bool *valid_range) const;
  const std::vector<post_content> &extract_post_data_items();

  bool valid_req = true; // assume it is valid from start
public:
  http_request(char *buff, size_t length);
  std::vector<post_content> items{};

  http_request(const http_request &) = delete;
  http_request &operator=(const http_request &) = delete;

  ~http_request();
};

#endif