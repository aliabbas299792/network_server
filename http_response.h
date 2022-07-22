#ifndef HTTP_RESP
#define HTTP_RESP

#include <string>

#include "utility.h"
#include "vendor/sha1/sha1.hpp"

constexpr const char *CRLF = "\r\n";
constexpr const char *websocket_magic_key =
    "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

class http_response {
private:
public:
  enum class http_ver { HTTP_10, HTTP_11 };
  enum class http_resp_codes {
    RESP_101,
    RESP_200,
    RESP_404,
    RESP_502,
    RESP_505
  };

  std::string header_start_line(http_resp_codes response_code, http_ver ver) {
    std::string http_start_line{};
    switch (ver) {
    case http_ver::HTTP_10:
      http_start_line += "HTTP/1.0";
      break;
    case http_ver::HTTP_11:
      http_start_line += "HTTP/1.1";
      break;
    }

    http_start_line += " ";

    switch (response_code) {
    case http_resp_codes::RESP_101:
      http_start_line += "Switching Protocols";
      break;
    case http_resp_codes::RESP_200:
      http_start_line += "OK";
      break;
    case http_resp_codes::RESP_404:
      http_start_line += "Not Found";
      break;
    case http_resp_codes::RESP_502:
      http_start_line += "Bad Gateway";
      break;
    case http_resp_codes::RESP_505:
      http_start_line += "HTTP Version not supported";
      break;
    }

    return http_start_line;
  }

  std::string make_top_of_header(http_resp_codes response_code, http_ver ver) {
    auto header = header_start_line(response_code, ver);

    switch (response_code) {

    case http_resp_codes::RESP_101:
      header += "Upgrade: websocket";
      header += CRLF;
      header += "Connection: Upgrade";
      header += CRLF;
      return header;
    case http_resp_codes::RESP_200:
      header += "Cache-Control: no-cache, no-store, must-revalidate";
      header += CRLF;
      header += "Pragma: no-cache";
      header += CRLF;
      header += "Expires: 0";
      header += CRLF;
    case http_resp_codes::RESP_404:
      header += "Connection: close";
      header += CRLF;
      header += "Keep-Alive: timeout=0, max=0";
      header += CRLF;
    case http_resp_codes::RESP_502:
    case http_resp_codes::RESP_505:
      return header;
    }
  }

  std::string get_accept_header_value(std::string sec_websocket_key) {
    sec_websocket_key += websocket_magic_key;

    SHA1 checksum{};
    checksum.update(sec_websocket_key);
    const std::string hash = checksum.final();

    return utility::b64_encode(hash.c_str());
  }

  http_response(http_resp_codes response_code, http_ver ver,
                std::string sec_websocket_key) {
    auto header = make_top_of_header(response_code, ver);
    auto ws_header_value = get_accept_header_value(sec_websocket_key);

    header += "Sec-WebSocket-Accept: ";
    header += ws_header_value;
    header += CRLF;
    header += CRLF;
  }

  http_response(http_resp_codes response_code, http_ver ver, bool accept_bytes,
                std::string content_type, size_t content_length) {
    auto header = make_top_of_header(response_code, ver);

    if (accept_bytes) {
      header += content_type;
      header += "Accept-Ranges: bytes";
      header += CRLF;
      header += "Content-Length: ";
      header += content_length;
      header += CRLF;
      header += "Range: bytes=0-";
      header += content_length;
      header += "/";
    } else {
      header += content_type;
      header += "Content-Length: ";
    }

    header += content_length;
    header += CRLF;
    header += CRLF;
  }
};

#endif