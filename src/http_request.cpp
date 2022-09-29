#include "../header/http_request.h"
#include "../header/debug_mem_ops.hpp"
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
    this->range_str = (char *)MALLOC(strlen(val_ptr) + 1);
    strcpy(this->range_str, val_ptr);
  }
}

http_request::http_request(char *buff) {
  if (!buff)
    return;

  char *buff_ptr = buff;
  char *save_ptr;
  char *tok_ptr = strtok_r(buff_ptr, "\r\n", &save_ptr);

  buff_ptr = nullptr;

  if (tok_ptr != nullptr)
    req_type_data_parser(tok_ptr);

  while ((tok_ptr = strtok_r(buff_ptr, "\r\n", &save_ptr)) != nullptr) {
    http_header_parser(tok_ptr);
  }
}

http_request::~http_request() {
  FREE(range_str);
  range_str = nullptr;
}

ranges http_request::get_ranges(size_t max_size) {
  range *rs{};
  size_t rs_len{};
  if (range_parse(max_size, &rs, &rs_len))
    return {rs, rs_len};
  return {};
}

bool http_request::range_parse(size_t max_size, range **ranges, size_t *ranges_len) {
  if (!range_str)
    return false;
  if (strncmp(range_str, "none", strlen("none")) == 0)
    return false; // no range supported

  size_t num_commas = 0;
  for (size_t i = 0; i < strlen(range_str); i++)
    if (range_str[i] == ',')
      num_commas++;

  *ranges_len = num_commas + 1; // 0 commas implies 1 range, 1 implies 2 etc
  *ranges = (range *)MALLOC(sizeof(range) * (*ranges_len));

  int bytes_len = strlen("bytes=");
  int cpy_len = strlen(range_str) - bytes_len;
  char *range_cpy = new char[cpy_len];
  memcpy(range_cpy, range_str + bytes_len, cpy_len);

  int ranges_idx = 0;

  char *saveptr{}, *tok{}, *cpy_ptr = range_cpy;
  while ((tok = strtok_r(cpy_ptr, ", ", &saveptr))) {
    cpy_ptr = nullptr;

    if (tok[0] != '-') {
      char *saveptr2{}, *cpy_ptr2 = tok;
      char *range_start = strtok_r(cpy_ptr2, "-", &saveptr2);
      cpy_ptr2 = nullptr;
      char *range_end = strtok_r(cpy_ptr2, "-", &saveptr2);

      auto &r = (*ranges)[ranges_idx++];
      r.start = std::atoi(range_start);
      if (*range_start != '0' && r.start == 0)
        return false;
      if (!range_end) {
        r.end = max_size;
      } else {
        r.end = std::atoi(range_end);
        if (*range_end != '0' && r.end == 0)
          return false;
      }
    } else {
      if (strlen(tok) > 1) {
        size_t end_offset = std::atoi(&tok[1]);
        if (tok[1] != 0 && end_offset == 0)
          return false;
        auto &r = (*ranges)[ranges_idx++];
        r.start = max_size - end_offset;
        r.end = max_size;
      }
    }
  }

  return true;
}