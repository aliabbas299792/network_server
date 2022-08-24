#include "subprojects/event_manager/event_manager.hpp"
#include <cstdint>
#include <endian.h>
#include <iostream>

#include <cstring>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include "http_request.h"
#include "utility.h"

constexpr int BACKLOG = 256;

int setup_listener_socket(event_manager &ev, int port) {
  int listener_pfd;
  int listener_fd;
  int yes = 1;
  addrinfo hints, *server_info, *traverser;

  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;       // IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM; // tcp
  hints.ai_flags = AI_PASSIVE;     // use local IP

  if (getaddrinfo(NULL, std::to_string(port).c_str(), &hints, &server_info) !=
      0)
    utility::fatal_error("getaddrinfo");

  for (traverser = server_info; traverser != NULL;
       traverser = traverser->ai_next) {
    listener_pfd = ev.socket_create(
        traverser->ai_family, traverser->ai_socktype, traverser->ai_protocol);

    if (listener_pfd == -1)
      utility::fatal_error("socket construction");

    listener_fd = ev.get_pfd_data(listener_pfd).fd;

    if (setsockopt(listener_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) ==
        -1)
      // 2nd param (SOL_SOCKET) is saying to do it at the socket protocol
      // level, not TCP or anything else, just for the socket
      utility::fatal_error("setsockopt SO_REUSEADDR");

    if (setsockopt(listener_fd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes)) ==
        -1)
      utility::fatal_error("setsockopt SO_REUSEPORT");

    if (setsockopt(listener_fd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes)) ==
        -1)
      utility::fatal_error("setsockopt SO_KEEPALIVE");

    int keep_idle =
        100; // The time (in seconds) the connection needs to remain idle before
             // TCP starts sending keepalive probes, if the socket option
             // SO_KEEPALIVE has been set on this socket.  This option should
             // not be used in code intended to be portable.
    if (setsockopt(listener_fd, IPPROTO_TCP, TCP_KEEPIDLE, &keep_idle,
                   sizeof(keep_idle)) == -1)
      utility::fatal_error("setsockopt TCP_KEEPIDLE");

    int keep_interval =
        100; // The time (in seconds) between individual keepalive probes. This
             // option should not be used in code intended to be portable.
    if (setsockopt(listener_fd, IPPROTO_TCP, TCP_KEEPINTVL, &keep_interval,
                   sizeof(keep_interval)) == -1)
      utility::fatal_error("setsockopt TCP_KEEPINTVL");

    int keep_count = 5; // The maximum number of keepalive probes TCP should
                        // send before dropping the connection.  This option
                        // should not be used in code intended to be portable.
    if (setsockopt(listener_fd, IPPROTO_TCP, TCP_KEEPCNT, &keep_count,
                   sizeof(keep_count)) == -1)
      utility::fatal_error("setsockopt TCP_KEEPCNT");

    if (bind(listener_fd, traverser->ai_addr, traverser->ai_addrlen) ==
        -1) { // try to bind the socket using the address data supplied, has
              // internet address, address family and port in the data
      perror("bind");

      ev.close_pfd(listener_fd);
      continue; // not fatal, we can continue
    }

    break; // we got here, so we've got a working socket - so break
  }

  freeaddrinfo(server_info); // free the server_info linked list

  if (traverser ==
      NULL) // means we didn't break, so never got a socket made successfully
    utility::fatal_error("no socket made");

  if (listen(listener_fd, BACKLOG) == -1)
    utility::fatal_error("listen");

  return listener_pfd;
}

void accept_callback(event_manager *ev, int listener_pfd,
                     sockaddr_storage *user_data, socklen_t size,
                     uint64_t pfd) {
  uint8_t *buff = reinterpret_cast<uint8_t *>(std::calloc(4096, 1));

  ev->submit_read(pfd, buff, 4096);

  ev->submit_accept(listener_pfd);
}

void process_get_req(event_manager *ev, processed_data read_metadata,
                     uint64_t pfd) {}

void read_callback(event_manager *ev, processed_data read_metadata,
                   uint64_t pfd) {
  char *read_buff = (char *)read_metadata.buff;
  http_request req(read_buff); // buffer used for http request

  if (!req.valid_req) {
    // close the connection
    ev->submit_shutdown(pfd, SHUT_RD);
    return;
  }

  if (req.req_type == "GET") {
    process_get_req(ev, read_metadata, pfd);
  }
  // std::string str =
  //     "HTTP/2 200 OK\ncontent-type: text/html; charset=utf-8\n\nhello world";

  // char *write_buffer = (char *)malloc(str.size());
  // str.copy(write_buffer, str.size());

  // ev->submit_write(pfd, (uint8_t *)write_buffer, str.size());
}

void write_callback(event_manager *ev, processed_data write_metadata,
                    uint64_t pfd) {
  std::cout << "wrote " << write_metadata.op_res_num << " bytes of "
            << write_metadata.length << " bytes\n";

  free(write_metadata.buff);

  ev->submit_shutdown(pfd, SHUT_RD);
}

void shutdown_callback(event_manager *ev, int how, uint64_t pfd) {
  switch (how) {
  case SHUT_RD: {
    std::cout << "shut down stage 1 done\n";
    ev->submit_shutdown(pfd, SHUT_WR);
    break;
  }
  case SHUT_WR: {
    std::cout << "shut down stage 2 done\n";
    std::cout << ev->submit_close(pfd) << " is close code\n";
    ;
    break;
  };
  }
}

void close_callback(event_manager *ev, uint64_t pfd) {
  std::cout << "pfd closed: " << pfd << "\n";
}

int main() {

  std::cout << listener_pfd << "\n";

  ev.start();
}