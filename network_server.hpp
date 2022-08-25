#ifndef NETWORK_SERVER
#define NETWORK_SERVER

#include "utility.hpp"
#include <set>

class network_server; // forward declaration for application methods

struct read_data {
  size_t read_amount{};
  uint8_t *buffer{};

  read_data(size_t read_amount, uint8_t *buffer)
      : read_amount(read_amount), buffer(buffer) {}
  read_data() {}
};

struct write_data {};

enum operation_type { OTHER, APPLICATION_READ, APPLICATION_WRITE, NETWORK_READ, NETWORK_WRITE };

struct task {
  operation_type op_type{};

  uint8_t *buff{};
  size_t buff_length{};
  size_t progress{}; // i.e pos in buffer
};

// the term client num is synonymous with pfd (pseudo fd) for the application
// stuff
class application_methods {
public:
  virtual void accept_callback(int client_num) {}
  virtual void read_callback(read_data data, int client_num) {}
  virtual void write_callback(write_data data, int client_num) {}
  virtual void event_callback(uint64_t task_id, int client_num) {}
  virtual void close_callback(int client_num) {}

  virtual ~application_methods(){};

  network_server *ns{}; // application_methods must have access to network_server for it to work
};

class network_server : public server_methods {
private:
  std::vector<task> task_data{};
  std::set<int> task_freed_idxs{};
  int get_task();
  int get_task(uint8_t *buff, size_t length);
  void free_task(int task_id);
  
private:
  application_methods *callbacks{}; // callbacks for the application
  int listener_pfd{};

  // renamed additional_info to task_id since it'll be used as task_id here
  // modified interface for task_id to default to 0 if not provided
  void accept_callback(int listener_pfd, sockaddr_storage *user_data, socklen_t size,
                       uint64_t pfd, int op_res_num, uint64_t task_id = 0) override;
  void read_callback(processed_data read_metadata, uint64_t pfd, uint64_t task_id = 0) override;
  void write_callback(processed_data write_metadata, uint64_t pfd, uint64_t task_id = 0) override;
  void event_callback(int pfd, int op_res_num, uint64_t task_id = 0) override;
  void shutdown_callback(int how, uint64_t pfd, int op_res_num, uint64_t task_id = 0) override;
  void close_callback(uint64_t pfd, int op_res_num, uint64_t task_id = 0) override;

public:
  network_server(int port, application_methods *callbacks);
};

#endif