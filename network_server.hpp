#ifndef NETWORK_SERVER
#define NETWORK_SERVER

#include "utility.hpp"
#include <set>

class network_server; // forward declaration for application methods

struct buff_data {
  size_t read_amount{};
  uint8_t *buffer{};

  buff_data(size_t read_amount, uint8_t *buffer) : read_amount(read_amount), buffer(buffer) {}
  buff_data() {}
};

// accept doesn't need any information for what it is
enum operation_type { NONE, HTTP_READ, HTTP_WRITE, HTTP_CLOSE, WEBSOCKET_READ, WEBSOCKET_WRITE, WEBSOCKET_CLOSE, RAW_READ, RAW_WRITE, RAW_CLOSE, EVENT_READ };

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
  virtual void event_callback(uint64_t task_id, int client_num) {}
  
  virtual void raw_read_callback(buff_data data, int client_num) {}
  virtual void raw_write_callback(buff_data data, int client_num) {}
  virtual void raw_close_callback(int client_num) {}
  
  virtual void websocket_read_callback(buff_data data, int client_num) {}
  virtual void websocket_write_callback(buff_data data, int client_num) {}
  virtual void websocket_close_callback(int client_num) {}
  
  virtual void http_read_callback(buff_data data, int client_num) {}
  virtual void http_write_callback(buff_data data, int client_num) {}
  virtual void http_close_callback(int client_num) {}

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
  // modified interface for task_id to default to -1 if not provided
  // assumes 2^64-1 tasks is enough (first 2^64-1 are valid, the final one is reserved invalid)
  // close, event and shutdown don't need task_id since they will always finish in one call
  void accept_callback(int listener_pfd, sockaddr_storage *user_data, socklen_t size, uint64_t pfd,
                       int op_res_num, uint64_t task_id = -1) override;
  void read_callback(processed_data read_metadata, uint64_t pfd, uint64_t task_id = -1) override;
  void write_callback(processed_data write_metadata, uint64_t pfd, uint64_t task_id = -1) override;
  void event_callback(int pfd, int op_res_num, uint64_t additional_info = -1) override;
  void shutdown_callback(int how, uint64_t pfd, int op_res_num, uint64_t additional_info = -1) override;
  void close_callback(uint64_t pfd, int op_res_num, uint64_t additional_info = -1) override;

private:
  void accept_procedure();
  void close_procedure(int pfd); // need to deal with HTTP, WEBSOCKET, RAW and NONE type connections

public:
  network_server(int port, application_methods *callbacks);

  int websocket_broadcast(const std::ranges::input_range auto &container);
  int websocket_read(); // same as normal read but carries info about what connection type
  int websocket_write();
  int websocket_close();

  int http_read(); // same as normal read but carries info about what connection type
  int http_write();
  int http_close();

  int raw_read(); // same as normal read but carries info about what connection type
  int raw_write();
  int raw_close();
};

#endif