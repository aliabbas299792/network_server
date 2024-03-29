#include "example.hpp"
#include <bits/types/struct_iovec.h>
#include <cstdint>
#include <fcntl.h>
#include <string>
#include <unistd.h>
#include <filesystem>

void app_methods::http_read_callback(http_request &&req, int client_num, bool failed_req) {
  if (failed_req) {
    return;
  }

  if (req.req_type == "POST") {

    PRINT_DEBUG("path: " << req.path);
    if (req.path == "/upload_file") {
      auto items = req.extract_post_data_items();

      for (auto &item : items) {
        auto write_path = base_file_path + "/" + item.filename;
        auto write_fd = open(write_path.c_str(), O_CREAT | O_WRONLY, 0666);
        write(write_fd, item.buff, item.size);
        close(write_fd);

        PRINT_DEBUG("name: " << item.name << "\n" << "filename: " << item.filename << "\n" << "disposition: "
          << item.content_disposition << "\n" << "type: " << item.content_type << "\n" << "size: " << item.size << "\n");
      }
      PRINT_DEBUG("num items: " << items.size());
    }

    PRINT_DEBUG("\n\n" << req.content_length << " is the content length\n");

    const char str[] = "Hello world! Good to see you contact me";
    uint8_t *buff = (uint8_t *)MALLOC(strlen(str) + 1);
    strcpy((char *)buff, str);
    http_response resp{http_resp_codes::RESP_200_OK, http_ver::HTTP_10, "text/plain", strlen(str)};

    auto header_buff = resp.allocate_buffer();
    auto size = resp.length();

    iovec *iovs = (iovec *)MALLOC(sizeof(iovec) * 2);
    iovs[0].iov_base = header_buff;
    iovs[0].iov_len = size;
    iovs[1].iov_base = buff;
    iovs[1].iov_len = strlen(str);

    ns->http_writev(client_num, iovs, 2);
  } else if (req.req_type == "GET") {
    const auto filepath = base_file_path + "/" + req.path;
    
    if(req.path == "/getlisting") {
      std::string out = "";
      for(const auto &e : std::filesystem::directory_iterator(base_file_path)) {
        const auto fpath = e.path().string();
        out += fpath.substr(base_file_path.size());
        out += "\n";
      }
      http_response resp{http_resp_codes::RESP_200_OK, http_ver::HTTP_10, "text/plain", out};
      char *write_buff = resp.allocate_buffer();
      ns->http_write(client_num, write_buff, resp.length());
    } else {
      const auto errorfilepath = base_file_path + "/404.html";

      PRINT_DEBUG("filepath: " << filepath);
      auto ret = ns->http_send_file(client_num, filepath.c_str(), errorfilepath.c_str(), req);
      PRINT_DEBUG("(http send file) ret is: " << ret);
    }
  }
}

void app_methods::raw_read_callback(buff_data data, int client_num, bool failed_req) {}

void app_methods::http_write_callback(buff_data data, int client_num, bool failed_req) {
  if (failed_req) {
    FREE(data.buffer);
    return;
  }

  FREE(data.buffer);
}

void app_methods::http_writev_callback(struct iovec *data, size_t num_iovecs, int client_num,
                                       bool failed_req) {
  if (num_iovecs > 0) {
    PRINT_DEBUG("wrtiev happened, data len: " << data[1].iov_len);
    FREE(data[0].iov_base);
    FREE(data[1].iov_base);
    FREE(data);
  } else {
    PRINT_DEBUG("writev occurred, using http_send_file");
  }
}

void app_methods::http_close_callback(int client_num) {
  PRINT_DEBUG("connection closed: " << client_num << " at time " << (unsigned long)time(NULL));
  MEM_PRINT();
}

int main() { app_methods{"/home/watcher/network_server/public"}; }
