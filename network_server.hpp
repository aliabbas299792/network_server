#ifndef NETWORK_SERVER
#define NETWORK_SERVER

#include "metadata.hpp"
#include "subprojects/event_manager/event_manager.hpp"
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
  RAW_WRITE,
  WEBSOCKET_CLOSE,
  HTTP_CLOSE,
  RAW_CLOSE
};

struct task {
  operation_type op_type{};

  uint8_t *buff{};
  size_t buff_length{};
  size_t progress{}; // i.e pos in buffer

  char shutdown = false;       // for net sockets
  char last_read_zero = false; // for net sockets
};

// the term client num is synonymous with pfd (pseudo fd) for the application
// stuff
class application_methods {
public:
  virtual void accept_callback(int client_num) {}
  virtual void event_trigger_callback(int client_num, uint64_t additional_info) {}

  virtual void raw_read_callback(buff_data data, int client_num) {}
  virtual void raw_write_callback(buff_data data, int client_num) {}
  virtual void raw_close_callback(int client_num) {}

  virtual void websocket_read_callback(buff_data data, int client_num) {}
  virtual void websocket_write_callback(buff_data data, int client_num) {}
  virtual void websocket_close_callback(int client_num) {}

  virtual void http_read_callback(buff_data data, int client_num) {}
  virtual void http_write_callback(buff_data data, int client_num) { std::cout << "http write success\n"; }
  virtual void http_close_callback(int client_num) {}

  // there is no way to async close an eventfd, if this is triggered it is due
  // to an eventfd read being cancelled because an error occurred (like it was closed)
  virtual void event_error_close_callback(int client_num, uint64_t additional_info) {}

  virtual ~application_methods(){};

  network_server *ns{}; // application_methods must have access to network_server for it to work
};

class network_server : public server_methods {
private:
  friend class web_methods;

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
                       int op_res_num, uint64_t additional_info = -1) override;
  void read_callback(processed_data read_metadata, uint64_t pfd, uint64_t task_id = -1) override;
  void write_callback(processed_data write_metadata, uint64_t pfd, uint64_t task_id = -1) override;
  void event_callback(int pfd, int op_res_num, uint64_t additional_info = -1) override;
  void shutdown_callback(int how, uint64_t pfd, int op_res_num, uint64_t task_id = -1) override;
  void close_callback(uint64_t pfd, int op_res_num, uint64_t task_id = -1) override;

private:
  // need to deal with HTTP, WEBSOCKET, RAW and NONE type connections
  void application_close_callback(int pfd, int task_id);
  void shutdown_procedure(int pfd);
  void close_pfd_gracefully(int pfd, uint64_t task_id); // will call shutdown if needed

  void network_read_procedure(int pfd, buff_data data) {
    auto task_id = get_task();
    // std::cout << "new task id " << task_id << "\n";
    const char *msg = "HTTP/2 200 OK\ncontent-type: text/html; charset=utf-8\n\nhello world";
    uint8_t *buff = new uint8_t[strlen(msg)];
    strcpy((char *)buff, msg);
    auto &task = task_data[task_id];
    task.op_type = HTTP_WRITE;
    task.buff = buff;
    task.buff_length = strlen(msg);
    ev->submit_write(pfd, (uint8_t *)msg, strlen(msg), task_id);
    std::cout << "i am writing task id " << task_data[task_id].buff << "\n";
    // if HTTP do HTTP thing
    // if websocket negotiation do negotiation
    // else websocket frames and must be active websocket
    // otherwise close the connection
  }

public:
  network_server(int port, event_manager *ev, application_methods *callbacks);

  // don't need HTTP or WEBSOCKET read because
  // they are autosubmitted

  template <int_range T> int websocket_broadcast(const T &container, buff_data data) {
    for (const auto pfd : container) {
      // ev->queue_write(pfd, uint8_t *buffer, size_t length)
    }
    return 1;
  }

  int websocket_write(int pfd, buff_data data) {
    auto task_id = get_task();
    auto &task = task_data[task_id];
    task.op_type = operation_type::WEBSOCKET_WRITE;

    // make a new buffer which is slightly bigger
    // add in websocket headings and stuff in there

    return ev->submit_write(pfd, data.buffer, data.size);
  }

  int websocket_close(int pfd) {
    auto task_id = get_task();
    auto &task = task_data[task_id];
    task.op_type = operation_type::WEBSOCKET_CLOSE;

    close_pfd_gracefully(pfd, task_id);
    return 0;
  }

  // read not reading since this is auto submitted
  int http_write(int pfd, buff_data data) {
    auto task_id = get_task();
    auto &task = task_data[task_id];
    task.op_type = operation_type::HTTP_WRITE;

    // make a new buffer which is slightly bigger
    // add in http headings and stuff in there

    return ev->submit_write(pfd, data.buffer, data.size);
  }

  int http_close(int pfd) {
    auto task_id = get_task();
    auto &task = task_data[task_id];
    task.op_type = operation_type::HTTP_CLOSE;

    close_pfd_gracefully(pfd, task_id);
    return 0;
  }

  // same as normal read but carries info about what connection type
  int raw_read(int pfd, buff_data data) {
    auto task_id = get_task();
    auto &task = task_data[task_id];
    task.op_type = operation_type::RAW_WRITE;

    return ev->submit_read(pfd, data.buffer, data.size);
  }

  int raw_write(int pfd, buff_data data) {
    auto task_id = get_task();
    auto &task = task_data[task_id];
    task.op_type = operation_type::RAW_WRITE;

    return ev->submit_write(pfd, data.buffer, data.size);
  }

  int raw_close(int pfd) {
    auto task_id = get_task();
    auto &task = task_data[task_id];
    task.op_type = operation_type::RAW_CLOSE;

    close_pfd_gracefully(pfd, task_id);
    return 0;
  }

  int eventfd_open();
  int eventfd_trigger(int pfd);
  int eventfd_prepare(int pfd);

  int local_open();
  int local_stat();
  int local_fstat();
  int local_unlink();
};

#endif