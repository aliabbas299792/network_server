#ifndef NETWORK_SERVER
#define NETWORK_SERVER

#include "../subprojects/event_manager/event_manager.hpp"
#include "http_request.h"
#include "metadata.hpp"
#include "utility.hpp"
#include <cstring>
#include <set>

class network_server; // forward declaration for application methods
class web_methods;

struct buff_data {
  size_t size{};
  uint8_t *buffer{};

  buff_data(size_t size, uint8_t *buffer) : size(size), buffer(buffer) {}
  buff_data() {}
};

template <typename T>
concept int_range = requires {
  std::ranges::input_range<int>;
};

// accept doesn't need any information for what it is
// websocket read and http read are under network read
enum operation_type {
  NETWORK_READ,
  WEBSOCKET_READ, // only used to get correct close callback
  RAW_READ,
  EVENT_READ,
  WEBSOCKET_WRITE,
  HTTP_WRITE,
  HTTP_WRITEV,
  RAW_WRITE,
  WEBSOCKET_CLOSE,
  HTTP_CLOSE,
  RAW_CLOSE
};

// describes each submitted task
struct task {
  operation_type op_type{};

  uint8_t *buff{};
  size_t buff_length = -1;
  size_t progress{}; // i.e pos in buffer

  struct iovec *iovs{};   // for storing original iovecs
  size_t num_iovecs = -1; // for writev

  char shutdown = false;       // for net sockets
  char last_read_zero = false; // for net sockets
};

// the term client num is synonymous with pfd (pseudo fd) for the application
// stuff
class application_methods {
protected:
  network_server *ns{}; // application_methods must have access to network_server for it to work
public:
  virtual void accept_callback(int client_num) {}
  virtual void event_trigger_callback(int client_num, uint64_t additional_info) {}

  virtual void raw_read_callback(buff_data data, int client_num) {}
  virtual void raw_write_callback(buff_data data, int client_num) {}
  virtual void raw_close_callback(int client_num) {}

  virtual void websocket_read_callback(buff_data data, int client_num) {}
  virtual void websocket_write_callback(buff_data data, int client_num) {}
  virtual void websocket_close_callback(int client_num) {}

  virtual void http_read_callback(http_request req, int client_num) {}
  virtual void http_write_callback(buff_data data, int client_num) {}
  virtual void http_writev_callback(struct iovec *data, size_t num_iovecs, int client_num) {}
  virtual void http_close_callback(int client_num) {}

  // there is no way to async close an eventfd, if this is triggered it is due
  // to an eventfd read being cancelled because an error occurred (like it was closed)
  virtual void event_error_close_callback(int client_num, uint64_t additional_info) {}

  virtual ~application_methods(){};

  void set_network_server(network_server *ns) { this->ns = ns; }
};

class network_server : public server_methods {
private:
  friend class web_methods;

  std::vector<task> task_data{};
  std::set<int> task_freed_idxs{};
  int get_task();
  int get_task(operation_type type, uint8_t *buff, size_t length);
  int get_task(operation_type type, struct iovec *iovecs, size_t num_iovecs);
  void free_task(int task_id);

private:
  application_methods *callbacks{}; // callbacks for the application
  int listener_pfd{};

  // renamed additional_info to task_id since it'll be used as task_id here
  // modified interface for task_id to default to -1 if not provided
  // assumes 2^64-1 tasks is enough (first 2^64-1 are valid, the final one is reserved invalid)
  // close, event and shutdown don't need task_id since they will always finish in one call
  void accept_callback(int listener_pfd, sockaddr_storage *user_data, socklen_t size, uint64_t pfd,
                       int op_res_num, uint64_t additional_info = -1) override;
  void read_callback(processed_data read_metadata, uint64_t pfd, uint64_t task_id = -1) override;
  void write_callback(processed_data write_metadata, uint64_t pfd, uint64_t task_id = -1) override;
  void writev_callback(processed_data_vecs write_metadata, uint64_t pfd, uint64_t task_id = -1) override;
  void event_callback(int pfd, int op_res_num, uint64_t additional_info = -1) override;
  void shutdown_callback(int how, uint64_t pfd, int op_res_num, uint64_t task_id = -1) override;
  void close_callback(uint64_t pfd, int op_res_num, uint64_t task_id = -1) override;

private:
  // need to deal with various types operation_types
  void application_close_callback(int pfd, int task_id);
  void close_pfd_gracefully(int pfd, uint64_t task_id = -1); // will call shutdown if needed

  //
  bool http_response_method(int pfd, buff_data data);
  bool websocket_frame_response_method(int pfd, buff_data data);
  void network_read_procedure(int pfd, buff_data data);

public:
  network_server(int port, event_manager *ev, application_methods *callbacks);

  // don't need HTTP or WEBSOCKET read because
  // they are autosubmitted
  template <int_range T> int websocket_broadcast(const T &container, buff_data data);
  int websocket_write(int pfd, buff_data data);
  int websocket_close(int pfd);

  // read not reading since this is auto submitted
  int http_writev(int pfd, struct iovec *iovs, size_t num_iovecs) {
    auto task_id = get_task(operation_type::HTTP_WRITEV, iovs, num_iovecs);
    return ev->submit_writev(pfd, iovs, num_iovecs, task_id);
  }

  int http_write(int pfd, char *buff, size_t buff_length);
  int http_close(int pfd);

  // same as normal read but carries info about what connection type
  int raw_read(int pfd, buff_data data);
  int raw_write(int pfd, buff_data data);
  int raw_close(int pfd);

  int eventfd_open();
  int eventfd_trigger(int pfd);
  int eventfd_prepare(int pfd, uint64_t additional_info);

  int local_open(const char *pathname, int flags);
  int local_stat(const char *pathname, struct stat *buff);
  int local_fstat(int pfd, struct stat *buff);
  int local_unlink(const char *pathname);

  void start();
};

#endif