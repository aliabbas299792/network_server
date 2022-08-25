#ifndef NETWORK_SERVER
#define NETWORK_SERVER

#include "utility.hpp"

class network_server; // forward declaration for application methods

struct read_data {
  size_t total_read{};
  size_t read_this_time{};
  uint8_t *buffer{};

  read_data(size_t total_read, size_t read_this_time, uint8_t *buffer)
      : total_read(total_read), read_this_time(read_this_time), buffer(buffer) {}
};

struct write_data {};

enum operation_type { OTHER, APPLICATION_READ, APPLICATION_WRITE, NETWORK_READ, NETWORK_WRITE };

struct client_data {
  operation_type op_type{};
};

// the term client num is synonymous with pfd (pseudo fd) for the application
// stuff
class application_methods {
public:
  virtual void accept_callback(network_server *ns, int client_num) {}
  virtual void read_callback(network_server *ns, read_data data, int client_num) {}
  virtual void write_callback(network_server *ns, write_data data, int client_num) {}
  virtual void event_callback(network_server *ns, uint64_t additional_info, int client_num) {}
  virtual void close_callback(network_server *ns, int client_num) {}

  virtual ~application_methods(){};
};

class network_server : public server_methods {
private:
  std::vector<client_data> clients_data{};

private:
  application_methods *callbacks{}; // callbacks for the application

  event_manager ev{};
  int listener_pfd{};

  void accept_callback(event_manager *ev, int listener_pfd, sockaddr_storage *user_data, socklen_t size,
                       uint64_t pfd, int op_res_num) override;
  void read_callback(event_manager *ev, processed_data read_metadata, uint64_t pfd) override;
  void write_callback(event_manager *ev, processed_data write_metadata, uint64_t pfd) override;
  void event_callback(event_manager *ev, uint64_t additional_info, int pfd, int op_res_num) override;
  void shutdown_callback(event_manager *ev, int how, uint64_t pfd, int op_res_num) override;
  void close_callback(event_manager *ev, uint64_t pfd, int op_res_num) override;

public:
  network_server(int port, application_methods *callbacks);
};

#endif