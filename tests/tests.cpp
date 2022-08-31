#include "../header/http_response.h"
#include "../network_server.hpp"

constexpr int port = 4000;

char *test1_img{};
size_t len{};

class app_methods : public application_methods {
public:
  void http_read_callback(http_request req, int client_num) override;
  void http_write_callback(buff_data data, int client_num) override;
  void http_writev_callback(struct iovec *data, size_t num_iovecs, int client_num) override;
  void http_close_callback(int client_num) override;

  app_methods() {
    event_manager ev{};
    network_server ns{port, &ev, this};

    this->set_network_server(&ns);

    ns.start();
  }
};

struct iovec vecs[2]{};

void app_methods::http_read_callback(http_request req, int client_num) {
  std::string content = "<h1>Hello world!</h1>";
  http_response resp{http_resp_codes::RESP_200_OK, http_ver::HTTP_10, false, "image/jpeg", len};

  auto resp_data = resp.allocate_buffer();
  auto content_data = test1_img;

  vecs[0].iov_base = resp_data;
  vecs[0].iov_len = strlen(resp_data);
  vecs[1].iov_base = content_data;
  vecs[1].iov_len = len;

  ns->http_writev(client_num, vecs, 2);
}

void app_methods::http_write_callback(buff_data data, int client_num) {
  std::cout << "wrote data: " << data.size << "\n";
  FREE(data.buffer);
}

void app_methods::http_writev_callback(struct iovec *data, size_t num_iovecs, int client_num) {
  std::cout << "wrote num vecs: " << num_iovecs << "\n";
  for (size_t i = 0; i < num_iovecs; i++) {
    std::cout << "\tiov[" << i << "] size: " << data[i].iov_len << "\n";
  }
  FREE(data[0].iov_base); // first iov_base is for the header data that was allocated
}

void app_methods::http_close_callback(int client_num) {
  std::cout << "connection closed: " << client_num << " at time " << (unsigned long)time(NULL) << "\n";
  MEM_PRINT();
}

int main() {
  std::ifstream file("test2.jpeg", std::ios::binary | std::ios::ate);
  auto size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<char> buff(size);
  file.read(buff.data(), size);

  test1_img = buff.data();
  len = size;

  app_methods{};
}
