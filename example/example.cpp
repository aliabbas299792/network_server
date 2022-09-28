#include "example.hpp"
#include <bits/types/struct_iovec.h>
#include <cstdint>
#include <fcntl.h>
#include <unistd.h>

void app_methods::http_read_callback(http_request req, int client_num, bool failed_req) {
  if (failed_req) {
    return;
  }

  const auto filepath = base_file_path + "/" + req.path;
  const auto file_num = ns->local_open(filepath.c_str(), O_RDONLY);

  if (file_num < 0) {
    std::cout << "failed to find file\n";
    ns->http_close(client_num);
    return;
  }

  struct stat sb {};
  ns->local_fstat(file_num, &sb);
  const size_t size = sb.st_size;

  uint8_t *buff = (uint8_t *)MALLOC(size);
  ns->raw_read(file_num, {size, buff});

  std::cout << "getting file: " << filepath << ", size: " << size << ", pfd: " << file_num
            << ", for client: " << client_num << "\n";

  if (file_num_to_request_client_num.size() <= file_num) {
    file_num_to_request_client_num.resize(file_num + 1);
  }
  file_num_to_request_client_num[file_num].client_num = client_num;
  file_num_to_request_client_num[file_num].filepath = filepath;
}

void app_methods::raw_read_callback(buff_data data, int client_num, bool failed_req) {
  if (failed_req) {
    std::cout << "(((failed req))) file pfd: \n" << client_num;
    return;
  }
  auto &jobdata = file_num_to_request_client_num[client_num];

  std::string type = "text/html";
  if (jobdata.filepath.ends_with(".mp4")) {
    type = "video/mp4";
  }

  http_response resp{http_resp_codes::RESP_200_OK, http_ver::HTTP_10, false, type, data.size};
  auto resp_data = resp.allocate_buffer();
  auto content_data = data.buffer;

  iovec *vecs = (iovec *)MALLOC(sizeof(iovec) * 2);
  vecs[0].iov_base = resp_data;
  vecs[0].iov_len = strlen(resp_data);
  vecs[1].iov_base = content_data;
  vecs[1].iov_len = data.size;

  ns->raw_close(client_num);

  std::cout << "(filepath, size, req pfd, file pfd): (" << jobdata.filepath << ", " << data.size << ", "
            << jobdata.client_num << ", " << client_num << ")\n";

  auto res = ns->http_writev(jobdata.client_num, vecs, 2);
  std::cout << "got the file, res: " << res << "\n";
}

void app_methods::http_write_callback(buff_data data, int client_num, bool failed_req) {}

void app_methods::http_writev_callback(struct iovec *data, size_t num_iovecs, int client_num,
                                       bool failed_req) {
  std::cout << "wrtiev happened, data len: " << data[1].iov_len << "\n";

  FREE(data[0].iov_base);
  FREE(data[1].iov_base);
  FREE(data);
}

void app_methods::http_close_callback(int client_num) {
  std::cout << "connection closed: " << client_num << " at time " << (unsigned long)time(NULL) << "\n";
  MEM_PRINT();
}

int main() { app_methods{"/home/watcher/network_server/public"}; }
