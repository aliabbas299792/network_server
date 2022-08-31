#ifndef HTTP_RESP
#define HTTP_RESP

#include <cstring>
#include <string>

#include "../vendor/sha1/sha1.hpp"
#include "debug_mem_ops.hpp"
#include "utility.hpp"

constexpr const char *CRLF = "\r\n";
constexpr const char *websocket_magic_key = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

enum class http_ver { HTTP_10, HTTP_11 };
enum class http_resp_codes { RESP_101, RESP_200_OK, RESP_404, RESP_502, RESP_505 };

class http_response {
private:
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
    case http_resp_codes::RESP_200_OK:
      http_start_line += "200 OK";
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
    header += CRLF;

    switch (response_code) {

    case http_resp_codes::RESP_101:
      header += "Upgrade: websocket";
      header += CRLF;
      header += "Connection: Upgrade";
      header += CRLF;
      break;
    case http_resp_codes::RESP_200_OK:
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
      break;
    }

    return header;
  }

  std::string get_accept_header_value(std::string sec_websocket_key) {
    sec_websocket_key += websocket_magic_key;

    SHA1 checksum{};
    checksum.update(sec_websocket_key);
    const std::string hash = checksum.final();

    return utility::b64_encode(hash.c_str());
  }

private:
  std::string response_data{};

public:
  http_response(http_resp_codes response_code, http_ver ver, std::string sec_websocket_key) {
    response_data = make_top_of_header(response_code, ver);
    auto ws_header_value = get_accept_header_value(sec_websocket_key);

    response_data += "Sec-WebSocket-Accept: ";
    response_data += ws_header_value;
    response_data += CRLF;
    response_data += CRLF;
  }

  http_response(http_resp_codes response_code, http_ver ver, bool accept_bytes, std::string content_type,
                std::string content) {
    auto size_str = std::to_string(content.size());

    response_data = make_top_of_header(response_code, ver);
    response_data += "Content-Type: ";
    response_data += content_type;
    response_data += CRLF;

    if (accept_bytes) {
      response_data += "Accept-Ranges: bytes";
      response_data += CRLF;
      response_data += "Content-Length: ";
      response_data += size_str;
      response_data += CRLF;
      response_data += "Range: bytes=0-";
      response_data += size_str;
      response_data += "/";
    } else {
      response_data += "Content-Length: ";
    }

    response_data += size_str;
    response_data += CRLF;
    response_data += CRLF;

    response_data += content;
  }

  http_response(http_resp_codes response_code, http_ver ver, bool accept_bytes, std::string content_type,
                size_t content_size) {
    auto size_str = std::to_string(content_size);

    response_data = make_top_of_header(response_code, ver);
    response_data += "Content-Type: ";
    response_data += content_type;
    response_data += CRLF;

    if (accept_bytes) {
      response_data += "Accept-Ranges: bytes";
      response_data += CRLF;
      response_data += "Content-Length: ";
      response_data += size_str;
      response_data += CRLF;
      response_data += "Range: bytes=0-";
      response_data += size_str;
      response_data += "/";
    } else {
      response_data += "Content-Length: ";
    }

    response_data += size_str;
    response_data += CRLF;
    response_data += CRLF;
  }

  // must free later
  char *allocate_buffer() {
    char *buff = (char *)MALLOC(response_data.size() + 1);
    memcpy(buff, response_data.c_str(), response_data.size() + 1);
    return buff;
  }

  size_t length() { return response_data.length(); }
};

#endif