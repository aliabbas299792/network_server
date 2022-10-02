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
  range *ranges_array{};
  size_t array_len{};
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
  // private:
  std::string range_str{};
  bool range_parse(size_t max_size, range **ranges, size_t *ranges_len) const;

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

  ranges get_ranges(size_t max_size) const;

  bool valid_req = true; // assume it is valid from start
public:
  http_request(char *buff, size_t length);
  std::vector<post_content> items{};

  const std::vector<post_content> &extract_data_items() {
    size_t boundary_len = strlen(boundary.c_str());
    const char CRLF[] = "\r\n";
    size_t break_len = strlen(CRLF);
    size_t content_length_num = std::atoi(content_length);

    if (content_length_num == 0)
      return items;

    char *buff_ptr = const_cast<char *>(content);
    while (buff_ptr != nullptr && buff_ptr < content + content_length_num) {
      buff_ptr = strstr(buff_ptr, boundary.c_str());
      if (buff_ptr != nullptr && (buff_ptr + boundary_len) < (content + content_length_num)) {
        post_content data{};

        *(buff_ptr - break_len) = '\0';
        buff_ptr += boundary_len;
        char *next = strstr(buff_ptr, boundary.c_str());
        if (!next) {
          next = strstr(buff_ptr, boundary_last.c_str());
          // std::cout << next << "\n";
          // *(next - break_len) = '\0';
          if (!next)
            next = const_cast<char *>(buff_ptr) + content_length_num - 1;
        }

        char *content = strstr(buff_ptr, "\r\n\r\n") + strlen("\r\n\r\n");

        const char content_disposition_key[] = "Content-Disposition: ";
        const char name_key[] = "name=\"";
        const char filename_key[] = "filename=\"";
        const char content_type_key[] = "Content-Type: ";

        char *substr_ptr = buff_ptr;
        if ((substr_ptr = strstr(buff_ptr, content_disposition_key)) != nullptr && substr_ptr < next) {
          substr_ptr += strlen(content_disposition_key);
          data.content_disposition = substr_ptr;
          if ((substr_ptr = strchr(substr_ptr, ';')) != nullptr) {
            *substr_ptr = '\0';
          }
          substr_ptr++;
        }
        if ((substr_ptr = strstr(substr_ptr, name_key)) != nullptr && substr_ptr < next) {
          substr_ptr += strlen(name_key);
          data.name = substr_ptr;
          if ((substr_ptr = strchr(substr_ptr, '"')) != nullptr) {
            *substr_ptr = '\0';
          }
          substr_ptr++;
        }
        if ((substr_ptr = strstr(substr_ptr, filename_key)) != nullptr && substr_ptr < next) {
          substr_ptr += strlen(filename_key);
          data.filename = substr_ptr;
          if ((substr_ptr = strchr(substr_ptr, '"')) != nullptr) {
            *substr_ptr = '\0';
          }
        }
        if ((substr_ptr = strstr(buff_ptr, content_type_key)) != nullptr && substr_ptr < next) {
          substr_ptr += strlen(content_type_key);
          data.content_type = substr_ptr;
          if ((substr_ptr = strchr(substr_ptr, '\r')) != nullptr) {
            *substr_ptr = '\0';
          }
        }

        data.size = next - content - break_len; // since next will be on a new line
        data.buff = reinterpret_cast<uint8_t *>(content);

        items.push_back(std::move(data));
        buff_ptr = next - 1;
      }
    }

    return items;
  }

  http_request(const http_request &) = delete;
  http_request &operator=(const http_request &) = delete;

  ~http_request();
};

#endif