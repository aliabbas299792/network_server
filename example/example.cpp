#include "example.hpp"
#include <bits/types/struct_iovec.h>
#include <cstdint>
#include <fcntl.h>
#include <unistd.h>

void app_methods::http_read_callback(http_request &&req, int client_num, bool failed_req) {
  if (failed_req) {
    return;
  }

  const auto filepath = base_file_path + "/" + req.path;
  const auto errorfilepath = base_file_path + "/404.html";
  std::cout << "filepath: " << filepath << "\n";
  auto ret = ns->http_send_file(client_num, filepath.c_str(), errorfilepath.c_str(), req);

  std::cout << "ret is: " << ret << "\n";
}

void app_methods::raw_read_callback(buff_data data, int client_num, bool failed_req) {}

void app_methods::http_write_callback(buff_data data, int client_num, bool failed_req) {}

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
