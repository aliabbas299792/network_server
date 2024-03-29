#ifndef NETWORK_SERVER
#define NETWORK_SERVER

#include "subprojects/event_manager/event_manager.hpp"
#include "header/http_request.hpp"
#include "header/lru.hpp"

class network_server; // forward declaration for application methods
class web_methods;

const constexpr int CACHE_SIZE = 3;

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
  INOTIFY_READ,
  NETWORK_READ,
  WEBSOCKET_READ, // only used to get correct close callback
  RAW_READ,
  RAW_READV,
  EVENT_READ,
  WEBSOCKET_WRITE,
  WEBSOCKET_WRITEV,
  HTTP_WRITE,
  HTTP_WRITEV,
  HTTP_SEND_FILE,
  HTTP_POST_READ,
  RAW_WRITE,
  RAW_WRITEV,
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

  std::vector<range> write_ranges{};
  int write_ranges_idx{};
  std::string filepath{};
  void *additional_ptr{};

  uint64_t for_client_num = -1; // which client it is for
};

// the term client num is synonymous with pfd (pseudo fd) for the application
// stuff
class application_methods {
protected:
  network_server *ns{}; // application_methods must have access to network_server for it to work
public:
  virtual void event_trigger_callback(int client_num, uint64_t additional_info, bool failed_req = false) {}

  virtual void raw_read_callback(buff_data data, int client_num, bool failed_req = false) {}
  virtual void raw_readv_callback(struct iovec *data, size_t num_iovecs, int client_num,
                                  bool failed_req = false) {}
  virtual void raw_write_callback(buff_data data, int client_num, bool failed_req = false) {}
  virtual void raw_writev_callback(struct iovec *data, size_t num_iovecs, int client_num,
                                   bool failed_req = false) {}
  virtual void raw_close_callback(int client_num) {}

  virtual void websocket_read_callback(buff_data data, int client_num, bool failed_req = false) {}
  virtual void websocket_write_callback(buff_data data, int client_num, bool failed_req = false) {}
  virtual void websocket_writev_callback(struct iovec *data, size_t num_iovecs, int client_num,
                                         bool failed_req = false) {}
  virtual void websocket_close_callback(int client_num) {}

  virtual void http_read_callback(http_request &&req, int client_num, bool failed_req = false) {}
  virtual void http_write_callback(buff_data data, int client_num, bool failed_req = false) {}
  virtual void http_writev_callback(struct iovec *data, size_t num_iovecs, int client_num,
                                    bool failed_req = false) {}
  virtual void http_close_callback(int client_num) {}

  virtual ~application_methods(){};

  void set_network_server(network_server *ns) { this->ns = ns; }
};

class network_server : public server_methods {
private:
  friend class web_methods;

  lru_file_cache cache{CACHE_SIZE};

  std::vector<task> task_data{};
  std::set<int> task_freed_idxs{};
  int get_task();
  int get_task_buff_op(operation_type type, uint8_t *buff, size_t length);
  int get_task_vector_op(operation_type type, struct iovec *iovecs, size_t num_iovecs);
  void free_task(int task_id);

  // helper for http_send_file
  void http_send_file_writev_submit(uint64_t pfd, uint64_t task_id);
  int http_send_file_writev_submit_helper(int task_id, int client_pfd, bool using_ranges);
  int http_send_cached_file(int client_num, std::string filepath_str, const http_request &req);
  void http_send_file_writev_continue(uint64_t pfd, uint64_t task_id);
  void http_send_file_read_failed(uint64_t pfd, uint64_t task_id);
  void http_send_file_writev_finish(uint64_t pfd, uint64_t task_id, bool failed_req);

  // vector ops progress in similar way, so abstracted it to one function
  void vector_ops_progress(uint64_t task_id, int op_res_num, size_t total_progress);

private:
  application_methods *callbacks{}; // callbacks for the application
  int listener_pfd{};

  // renamed additional_info to task_id since it'll be used as task_id here
  // modified interface for task_id to default to -1 if not provided
  // assumes 2^64-1 tasks is enough (first 2^64-1 are valid, the final one is reserved invalid)
  // close and event don't need task_id since they will always finish in one call
  void accept_callback(int listener_pfd, sockaddr_storage *user_data, socklen_t size, uint64_t pfd,
                       int op_res_num, uint64_t additional_info = -1) override;
  void read_callback(processed_data read_metadata, uint64_t pfd, uint64_t task_id = -1) override;
  void write_callback(processed_data write_metadata, uint64_t pfd, uint64_t task_id = -1) override;
  void writev_callback(processed_data_vecs write_metadata, uint64_t pfd, uint64_t task_id = -1) override;
  void readv_callback(processed_data_vecs read_metadata, uint64_t pfd, uint64_t task_id = -1) override;
  void event_callback(int pfd, int op_res_num, uint64_t additional_info = -1) override;
  void close_callback(uint64_t pfd, int op_res_num, uint64_t task_id = -1) override;

private:
  // need to deal with various types operation_types
  void application_close_callback(int pfd, uint64_t task_id = -1);

  // helper methods
  bool http_response_method(int pfd, bool *should_auto_resubmit_read, buff_data data = {}, bool failed_req = false);
  bool websocket_frame_response_method(int pfd, bool *should_auto_resubmit_read, buff_data data = {}, bool failed_req = false);
  void network_read_procedure(int pfd, uint64_t task_id, bool *should_auto_resubmit_read, bool failed_req = false, buff_data data = {});

public:
  network_server(int port, event_manager *ev, application_methods *callbacks);

  // don't need HTTP or WEBSOCKET read because
  // they are autosubmitted
  template <int_range T> int websocket_broadcast(const T &client_num_container, buff_data data);
  int websocket_write(int client_num, buff_data data);
  int websocket_writev(int client_num, struct iovec *iovs, size_t num_iovecs);
  int websocket_close(int client_num);

  // read not reading since this is auto submitted
  int http_writev(int client_num, struct iovec *iovs, size_t num_iovecs);
  int http_write(int client_num, char *buff, size_t buff_length);
  int http_close(int client_num);

  int http_send_file(int client_num, const char *filepath, const char *not_found_filepath,
                     const http_request &req);

  // same as normal read but carries info about what connection type
  int raw_read(int client_num, buff_data data);
  int raw_readv(int client_num, struct iovec *iovs, size_t num_iovecs);
  int raw_write(int client_num, buff_data data);
  int raw_writev(int client_num, struct iovec *iovs, size_t num_iovecs);
  int raw_close(int client_num);

  int eventfd_open();
  int eventfd_trigger(int client_num);
  int eventfd_prepare(int client_num, uint64_t additional_info);

  // hands over responsibility of this fd to the network server
  // returned client_num is used with other operations with the network server
  int get_client_num_from_fd(int fd);

  void print_cache_stats() {
    cache.print_stats();
  }

  void start();
  void stop();
};

#endif