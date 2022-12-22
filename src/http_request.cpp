#include "header/http_request.hpp"
#include "header/debug_mem_ops.hpp"
#include <cstring>

void http_request::req_type_data_parser(char *token_str) {
  // the smallest valid get line
  const int min_length = strlen("GET / HTTP/1.1");

  if (strlen(token_str) < min_length) {
    valid_req = false;
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
    valid_req = false;
    return;
  }

  // advance by length of request
  token_str = &token_str[req_type.size() + 1];

  char *save_ptr;
  char *path = strtok_r(token_str, " ", &save_ptr);

  if (path == nullptr) {
    valid_req = false;
    return;
  }

  char *http_ver = strtok_r(nullptr, " ", &save_ptr);

  if (http_ver == nullptr) {
    valid_req = false;
    return;
  }

  if (strcmp(http_ver, HTTP_VER11) != 0 && strcmp(http_ver, HTTP_VER10) != 0) {
    valid_req = false;
    return;
  }

  if (strcmp(path, "/") == 0) {
    this->path = ROOT_FILE;
  } else {
    this->path = path;
  }

  save_ptr = nullptr;
  char *tok_ptr;
  char *path_ptr = path;

  while ((tok_ptr = strtok_r(path_ptr, "/", &save_ptr)) != nullptr) {
    path_ptr = nullptr;
    subdirs.push_back(tok_ptr);
  }
}

void http_request::http_header_parser(char *token_str) {
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

  char *val_ptr = strchr(header_value, ' ') + 1;
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
  } else if (strcmp(header_key, "Content-Length") == 0) {
    this->content_length = val_ptr;
  } else if (strcmp(header_key, "Content-Type") == 0) {
    this->content_type = val_ptr;
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
    this->range_str = val_ptr;
  }
}

const std::vector<post_content> &http_request::extract_post_data_items() {
  if (boundary_str == "" || boundary_last_str == "")
    return items;

  auto boundary = boundary_str.c_str();
  auto boundary_last = boundary_last_str.c_str();
  size_t content_length_num = std::atoi(content_length);

  if (content_length_num == 0)
    return items;

  void *item_ptr = content;
  char *max_ptr = (char *)item_ptr + content_length_num - 1;

  while ((item_ptr = memmem(item_ptr, max_ptr - (char *)item_ptr, boundary, boundary_str.size())) !=
         nullptr) {
    item_ptr = (char *)item_ptr + boundary_str.size(); // increment past the boundary
    char *data_ptr = (char *)item_ptr;

    if (data_ptr < max_ptr) {
      const char content_disposition_key[] = "Content-Disposition: ";
      const char name_key[] = "name=\"";
      const char filename_key[] = "filename=\"";
      const char content_type_key[] = "Content-Type: ";

      post_content data{};

      char *next =
          reinterpret_cast<char *>(memmem(data_ptr, max_ptr - data_ptr, boundary, boundary_str.size()));
      if (!next) {
        next = reinterpret_cast<char *>(
            memmem(data_ptr, max_ptr - data_ptr, boundary_last, boundary_last_str.size()));
        if (!next)
          next = max_ptr;
      }
      *(next - break_len) = '\0'; // inserts null character before next section

      size_t current_section_length = (next - break_len) - data_ptr - 1; // -1 for the null character

      void *substr_ptr{};

      if ((substr_ptr = memmem(item_ptr, max_ptr - data_ptr, double_CRLF, double_break_len)) != nullptr) {
        char *substr_data_ptr = (char *)substr_ptr;
        substr_data_ptr += double_break_len;
        data.buff = reinterpret_cast<uint8_t *>(substr_data_ptr);
        data.size = next - substr_data_ptr - break_len;
      }

      if ((substr_ptr = memmem(data_ptr, current_section_length, content_disposition_key,
                               strlen(content_disposition_key))) != nullptr) {
        char *substr_data_ptr = (char *)substr_ptr;
        substr_data_ptr += strlen(content_disposition_key);
        data.content_disposition = substr_data_ptr;
        if ((substr_data_ptr = strchr(substr_data_ptr, ';')) != nullptr) {
          *substr_data_ptr = '\0';
        }
      }

      if ((substr_ptr = memmem(data_ptr, current_section_length, name_key, strlen(name_key))) != nullptr) {
        char *substr_data_ptr = (char *)substr_ptr;
        substr_data_ptr += strlen(name_key);
        data.name = substr_data_ptr;
        if ((substr_data_ptr = strchr(substr_data_ptr, '"')) != nullptr) {
          *substr_data_ptr = '\0';
        }
      }

      if ((substr_ptr = memmem(data_ptr, current_section_length, filename_key, strlen(filename_key))) !=
          nullptr) {
        char *substr_data_ptr = (char *)substr_ptr;
        substr_data_ptr += strlen(filename_key);
        data.filename = substr_data_ptr;
        if ((substr_data_ptr = strchr(substr_data_ptr, '"')) != nullptr) {
          *substr_data_ptr = '\0';
        }
      }

      if ((substr_ptr = memmem(data_ptr, current_section_length, content_type_key,
                               strlen(content_type_key))) != nullptr) {
        char *substr_data_ptr = (char *)substr_ptr;
        substr_data_ptr += strlen(content_type_key);
        data.content_type = substr_data_ptr;
        if ((substr_data_ptr = strchr(substr_data_ptr, '\n')) != nullptr) {
          *substr_data_ptr = '\0';
        }
      }

      items.push_back(data);
    }
  }

  return items;
}

http_request::http_request(char *original_buff, size_t length) {
  if (!original_buff)
    return;

  // since strtok_r modifies the original buffer, we use a copy
  this->buff = (char *)MALLOC(length + 1); // +1 for '\0' at the end
  memcpy(this->buff, original_buff, length);

  this->content = strstr(buff, double_CRLF);

  if (this->content != nullptr && (this->content - this->buff) > (int64_t)double_break_len) {
    *this->content = '\0';             // to delimit
    this->content += double_break_len; // should point to content now
  } else {
    this->content = nullptr;
  }

  char *buff_ptr = buff;
  char *save_ptr;
  char *tok_ptr = strtok_r(buff_ptr, "\r\n", &save_ptr);

  buff_ptr = nullptr;

  if (tok_ptr != nullptr)
    req_type_data_parser(tok_ptr);

  while ((tok_ptr = strtok_r(buff_ptr, "\r\n", &save_ptr)) != nullptr) {
    http_header_parser(tok_ptr);
  }

  if (this->content_type) {
    const char boundary_tok[] = "boundary=";
    char *boundary_ptr = strstr((char *)this->content_type, boundary_tok);

    if (boundary_ptr && boundary_ptr + strlen(boundary_tok) < buff + length) {
      boundary_ptr += strlen(boundary_tok);
      auto ptr_if_space = strchr(boundary_ptr, ' ');
      auto ptr_if_end_of_str = boundary_ptr + strlen(boundary_ptr);

      auto end_ptr = ptr_if_end_of_str;
      if (!ptr_if_space)
        end_ptr = ptr_if_end_of_str;

      boundary_str = "--";
      boundary_str += std::string(boundary_ptr, end_ptr - boundary_ptr);
      boundary_last_str = boundary_str + "--";
      boundary_str += CRLF;
    }
  }
}

http_request::~http_request() {
  FREE(buff);
  buff = nullptr;
}

std::vector<range> http_request::get_ranges(size_t max_size, bool *valid_range) const {
  if(!valid_range) {
    std::cerr << "No valid range indicator variable passed\n";
    return {};
  }

  std::vector<range> ranges{};
  *valid_range = true; // assume true from the start

  if (range_str.length() == 0)
    return ranges;
  if (range_str == "none")
    return ranges; // no range supported

  int bytes_len = strlen("bytes=");
  int cpy_len = range_str.length() - bytes_len + 1; // +1 for null terminator
  char *range_cpy = new char[cpy_len];
  memcpy(range_cpy, range_str.c_str() + bytes_len, cpy_len);

  char *saveptr{}, *tok{}, *cpy_ptr = range_cpy;
  while ((tok = strtok_r(cpy_ptr, ", ", &saveptr))) {
    cpy_ptr = nullptr;

    size_t start{}, end{};

    if (tok[0] != '-') {
      char *saveptr2{}, *cpy_ptr2 = tok;
      char *range_start = strtok_r(cpy_ptr2, "-", &saveptr2);
      cpy_ptr2 = nullptr;
      char *range_end = strtok_r(cpy_ptr2, "-", &saveptr2);

      start = std::atoi(range_start);

      if (start >= max_size - 1) {
#ifdef VERBOSE_DEBUG
        std::cout << max_size << " is max size\n\n\n\n";
#endif
        *valid_range = false;
        break;
      }

      if (*range_start != '0' && start == 0)
        break;
      if (!range_end) {
        end = (max_size - 1);

        if (end > max_size - 1) {
          end = max_size - 1;
        }
      } else {
#ifdef VERBOSE_DEBUG
        std::cout << "\n\n\t\trange str: |" << range_str << "|\n";
        std::cout << "\t\trange start: |" << range_start << "|\n";
        std::cout << "\t\trange end is nullptr: |" << (range_end == nullptr) << "|\n";
        std::cout << "\t\trange end len: |" << strlen(range_end) << "|\n\n";
#endif
        end = std::atoi(range_end);
        if (*range_end != '0' && end == 0)
          break;
      }
    } else {
      if (strlen(tok) > 1) {
        size_t end_offset = std::atoi(&tok[1]);
        if (tok[1] != 0 && end_offset == 0)
          break;
        start = (max_size - 1) - end_offset;
        end = (max_size - 1);
      }
    }

    ranges.emplace_back(start, end);
  }

  return ranges;
}