#ifndef HTTP_REQ
#define HTTP_REQ

#include <array>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

constexpr char HTTP_VER[] = "HTTP/1.1";

class http_request {
private:
  void req_type_data_parser(char *token_str) {
    // the smallest valid get line
    const int min_length = strlen("GET / HTTP/1.1");
    char root_file[] = "index.html";

    if (strlen(token_str) < min_length) {
      return;
    }

    bool is_req_type = false;
    auto req_types = std::array{"GET", "PUT", "POST", "DELETE"};
    for (const auto &type : req_types) {
      if (strncmp(type, token_str, strlen(type)) == 0) {
        req_type = type;
        is_req_type = true;
      }
    }

    if (!is_req_type) {
      return;
    }

    // advance by length of request
    token_str = &token_str[req_type.size() + 1];

    char *save_ptr;
    char *path = strtok_r(token_str, " ", &save_ptr);

    if (path == nullptr) {
      return;
    }

    char *http_ver = strtok_r(nullptr, " ", &save_ptr);

    if (http_ver == nullptr) {
      return;
    }

    if (strcmp(http_ver, HTTP_VER) != 0) {
      return;
    }

    if (*path == '/' && strlen(path) == 1) {
      path = root_file;
    }

    this->path = path;

    save_ptr = nullptr;
    char *tok_ptr;
    char *path_ptr = path;

    while ((tok_ptr = strtok_r(path_ptr, "/", &save_ptr)) != nullptr) {
      path_ptr = nullptr;
      subdirs.push_back(tok_ptr);
    }
  }

  void http_header_parser(char *token_str) {
    // the smallest valid header line
    const int min_length = strlen("a:b");
    int str_len = strlen(token_str);

    if (str_len < min_length) {
      return;
    }

    char *save_ptr;
    char *header_key = strtok_r(token_str, ":", &save_ptr);

    if (header_key == nullptr) {
      return;
    }

    char *header_value = strtok_r(nullptr, ":", &save_ptr);

    if (header_value == nullptr) {
      return;
    }

    int header_value_str_len = strlen(header_value);

    char *val_ptr = strrchr(header_value, ' ') + 1;
    if (val_ptr >= header_value + header_value_str_len) {
      return;
    }

    if (strcmp(header_key, "Host") == 0) {
      this->host = val_ptr;
    } else if (strcmp(header_key, "User-Agent") == 0) {
      this->user_agent = val_ptr;
    } else if (strcmp(header_key, "Accept") == 0) {
      this->accept = val_ptr;
    } else if (strcmp(header_key, "Accept-Language") == 0) {
      this->accept_language = val_ptr;
    } else if (strcmp(header_key, "Accept-Encoding") == 0) {
      this->accept_encoding = val_ptr;
    } else if (strcmp(header_key, "DNT") == 0) {
      this->dnt = val_ptr;
    } else if (strcmp(header_key, "Connection") == 0) {
      this->connection = val_ptr;
    } else if (strcmp(header_key, "Upgrade-Insecure-Requests") == 0) {
      this->upgrade_insecure_requests = val_ptr;
    } else if (strcmp(header_key, "Upgrade") == 0) {
      this->upgrade = val_ptr;
    } else if (strcmp(header_key, "Sec-WebSocket-Key") == 0) {
      this->sec_websocket_key = val_ptr;
    } else if (strcmp(header_key, "X-Forwarded-For") == 0) {
      this->x_forwarded_for = val_ptr;
    } else if (strcmp(header_key, "Range") == 0) {
      this->range = val_ptr;
    }
  }

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
  http_request(char *buff) {
    char *buff_ptr = buff;
    char *save_ptr;
    char *tok_ptr;

    while ((tok_ptr = strtok_r(buff_ptr, "\r\n", &save_ptr)) != nullptr) {
      buff_ptr = nullptr;

      req_type_data_parser(tok_ptr);
      http_header_parser(tok_ptr);
    }

    if (path == "")
      valid_req = false;
  }
};

#endif