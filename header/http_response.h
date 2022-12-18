#ifndef HTTP_RESP
#define HTTP_RESP

#include <cstring>
#include <string>

#include "vendor/sha1/sha1.hpp"
#include "debug_mem_ops.hpp"
#include "http_request.h"
#include "utility.hpp"

constexpr const char *CRLF = "\r\n";
constexpr const char *websocket_magic_key = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

enum class http_ver { HTTP_10, HTTP_11 };
enum class http_resp_codes {
  RESP_101,
  RESP_200_OK,
  RESP_206_PARTIAL,
  RESP_404,
  RESP_416_UNSATISFIABLE_RANGE,
  RESP_502,
  RESP_505
};

class http_response {
private:
  std::string header_start_line(http_resp_codes response_code, http_ver ver);
  std::string make_top_of_header(http_resp_codes response_code, http_ver ver);
  std::string get_accept_header_value(std::string sec_websocket_key);

private:
  std::string response_data{};

public:
  http_response(http_resp_codes response_code, http_ver ver, std::string sec_websocket_key);

  http_response(http_resp_codes response_code, http_ver ver, std::string content_type, std::string content);

  http_response(http_resp_codes response_code, http_ver ver, std::string content_type, size_t content_size);

  http_response(http_resp_codes response_code, http_ver ver, const range &write_range,
                std::string content_type, size_t content_size);

  char *allocate_buffer(); // must free later
  static std::string get_type_from_filename(std::string filename);
  size_t length();
};

#endif