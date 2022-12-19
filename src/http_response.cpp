#include "header/http_response.hpp"

std::string http_response::header_start_line(http_resp_codes response_code, http_ver ver) {
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
  case http_resp_codes::RESP_206_PARTIAL:
    http_start_line += "206 Partial Content";
    break;
  case http_resp_codes::RESP_404:
    http_start_line += "Not Found";
    break;
  case http_resp_codes::RESP_416_UNSATISFIABLE_RANGE:
    http_start_line += "416 Range Not Satisfiable";
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

std::string http_response::make_top_of_header(http_resp_codes response_code, http_ver ver) {
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
  case http_resp_codes::RESP_206_PARTIAL:
  case http_resp_codes::RESP_416_UNSATISFIABLE_RANGE:
  case http_resp_codes::RESP_502:
  case http_resp_codes::RESP_505:
    break;
  }

  return header;
}

std::string http_response::get_accept_header_value(std::string sec_websocket_key) {
  sec_websocket_key += websocket_magic_key;

  SHA1 checksum{};
  checksum.update(sec_websocket_key);
  const std::string hash = checksum.final();

  return utility::b64_encode(hash.c_str());
}

http_response::http_response(http_resp_codes response_code, http_ver ver, std::string sec_websocket_key) {
  response_data = make_top_of_header(response_code, ver);
  auto ws_header_value = get_accept_header_value(sec_websocket_key);

  response_data += "Sec-WebSocket-Accept: ";
  response_data += ws_header_value;
  response_data += CRLF;
  response_data += CRLF;
}

http_response::http_response(http_resp_codes response_code, http_ver ver, std::string content_type,
                             std::string content) {
  auto size_str = std::to_string(content.size());

  response_data = make_top_of_header(response_code, ver);
  response_data += "Content-Type: ";
  response_data += content_type;
  response_data += CRLF;
  response_data += size_str;
  response_data += CRLF;
  response_data += CRLF;

  response_data += content;
}

http_response::http_response(http_resp_codes response_code, http_ver ver, std::string content_type,
                             size_t content_size) {
  auto size_str = std::to_string(content_size);

  response_data = make_top_of_header(response_code, ver);
  response_data += "Content-Type: ";
  response_data += content_type;
  response_data += CRLF;
  response_data += "Content-Length: ";
  response_data += size_str;
  response_data += CRLF;
  response_data += CRLF;
}

http_response::http_response(http_resp_codes response_code, http_ver ver, const range &write_range,
                             std::string content_type, size_t content_size) {
  response_data = make_top_of_header(response_code, ver);
  response_data += "Content-Type: ";
  response_data += content_type;
  response_data += CRLF;
  response_data += "Content-Length: ";
  response_data += std::to_string(1 + write_range.end - write_range.start);
  response_data += CRLF;
  response_data += "Accept-Ranges: bytes";
  response_data += CRLF;
  response_data += "Content-Range: bytes ";
  response_data += std::to_string(write_range.start);
  response_data += "-";
  response_data += std::to_string(write_range.end);
  response_data += "/";
  response_data += std::to_string(content_size);
  response_data += CRLF;
  response_data += CRLF;
}

char *http_response::allocate_buffer() {
  char *buff = (char *)MALLOC(response_data.size() + 1);
  memcpy(buff, response_data.c_str(), response_data.size() + 1);
  return buff;
}

size_t http_response::length() { return response_data.length(); }

std::string http_response::get_type_from_filename(std::string filename) {
  std::string type = "text/html";
  if (filename.ends_with(".mp4")) {
    type = "video/mp4";
  } else if (filename.ends_with(".jpg")) {
    type = "image/jpg";
  } else if (filename.ends_with(".gif")) {
    type = "image/gif";
  }
  return type;
}