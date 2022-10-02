#include "example.hpp"
#include <bits/types/struct_iovec.h>
#include <cstdint>
#include <fcntl.h>
#include <string>
#include <unistd.h>

void app_methods::http_read_callback(http_request &&req, int client_num, bool failed_req) {
  if (failed_req) {
    return;
  }

  if (req.req_type == "POST") {

    std::cout << "path: " << req.path << "\n";
    if (req.path == "/upload_file") {
      auto items = req.extract_post_data_items();

      for (auto &item : items) {
        auto write_path = base_file_path + "/" + item.filename;
        auto write_fd = open(write_path.c_str(), O_CREAT | O_WRONLY, 0666);
        write(write_fd, item.buff, item.size);
        close(write_fd);

        if (item.name)
          std::cout << "name: " << item.name << "\n";
        if (item.filename)
          std::cout << "filename: " << item.filename << "\n";
        if (item.content_disposition)
          std::cout << "disposition: " << item.content_disposition << "\n";
        if (item.content_type)
          std::cout << "type: " << item.content_type << "\n";
        std::cout << "size: " << item.size << "\n\n";
      }

      std::cout << "num items: " << items.size() << "\n";
    }
    std::cout << "\n\n" << req.content_length << " is the content length\n\n";

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
    const auto errorfilepath = base_file_path + "/404.html";
    std::cout << "filepath: " << filepath << "\n";
    auto ret = ns->http_send_file(client_num, filepath.c_str(), errorfilepath.c_str(), req);

    std::cout << "ret is: " << ret << "\n";
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
    std::cout << "wrtiev happened, data len: " << data[1].iov_len << "\n";

    FREE(data[0].iov_base);
    FREE(data[1].iov_base);
    FREE(data);
  } else {
    std::cout << "writev occurred, using http_send_file\n";
  }
}

void app_methods::http_close_callback(int client_num) {
  std::cout << "connection closed: " << client_num << " at time " << (unsigned long)time(NULL) << "\n";
  MEM_PRINT();
}

int main() { app_methods{"/home/watcher/network_server/public"}; }
