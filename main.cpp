// // #include "network_server.hpp"
// #include "subprojects/event_manager/event_manager.hpp"
// #include "utility.hpp"
// #include <cstring>

// constexpr int port = 4000;

// // class test_app : public application_methods {
// // public:
// //   void http_write_callback(buff_data data, int client_num) override {
// //     std::cout << "delivered to client " << client_num << "\n";
// //   }
// // };

// class test_server : public server_methods {
// public:
//   void accept_callback(int listener_pfd, sockaddr_storage *user_data, socklen_t size, uint64_t pfd,
//                        int op_res_num, uint64_t additional_info = -1) override {
//     auto buff = new uint8_t[READ_SIZE];

//     ev->submit_read(pfd, buff, READ_SIZE);

//     ev->submit_accept(listener_pfd);
//   }

// void read_callback(processed_data read_metadata, uint64_t pfd, uint64_t task_id = -1) override {
//   const char *msg = "HTTP/2 200 OK\ncontent-type: text/html; charset=utf-8\n\nhello world";
//   ev->submit_write(pfd, (uint8_t *)msg, strlen(msg));

//   free(read_metadata.buff);
// }

//   void write_callback(processed_data write_metadata, uint64_t pfd, uint64_t task_id = -1) override {
//     std::cout << "got write callback\n";

//     ev->close_pfd(pfd);
//   }

//   void event_callback(int pfd, int op_res_num, uint64_t additional_info = -1) override {}

//   void shutdown_callback(int how, uint64_t pfd, int op_res_num, uint64_t task_id = -1) override {}

//   void close_callback(uint64_t pfd, int op_res_num, uint64_t task_id = -1) override {}
// };

// int main() {
//   test_server server{};
//   event_manager ev{};

//   ev.set_server_methods(&server);
//   server.set_event_manager(&ev);

//   auto listener_pfd = utility::setup_listener_pfd(port, &ev);
//   ev.submit_accept(listener_pfd);

//   ev.start();

//   // network_server server{port, &ev, &app};
// }

#include "network_server.hpp"
#include "subprojects/event_manager/event_manager.hpp"

constexpr int port = 4000;

int main() {
  event_manager ev{};
  application_methods am{};

  network_server server{port, &ev, &am};
}
