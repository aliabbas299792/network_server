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

class http_request {
private:
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
  char *range{};

  bool valid_req = true; // assume it is valid from start
public:
  http_request(char *buff);
};

#endif