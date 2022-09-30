#ifndef NETWORK_SERVER
#define NETWORK_SERVER

#include "../subprojects/event_manager/event_manager.hpp"
#include "header/debug_mem_ops.hpp"
#include "header/http_request.h"
#include "header/metadata.hpp"
#include "header/utility.hpp"
#include <cstdint>
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
  RAW_READV,
  EVENT_READ,
  WEBSOCKET_WRITE,
  WEBSOCKET_WRITEV,
  HTTP_WRITE,
  HTTP_WRITEV,
  HTTP_SEND_FILE,
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

  ranges write_ranges{};
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
  virtual void accept_callback(int client_num) {}
  virtual void event_trigger_callback(int client_num, uint64_t additional_info) {}

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

  // there is no way to async close an eventfd, if this is triggered it is due
  // to an eventfd read being cancelled because an error occurred (like it was closed)
  virtual void event_error_close_callback(int client_num, uint64_t additional_info) {}

  virtual ~application_methods(){};

  void set_network_server(network_server *ns) { this->ns = ns; }
};

// pfd_state is needed for storing any necessary state information
// for now this is just used for shutting connections gracefully
struct pfd_state {
  char shutdown_done = false;  // for net sockets
  char last_read_zero = false; // for net sockets
};

class network_server : public server_methods {
private:
  friend class web_methods;

  std::vector<pfd_state> pfd_states{};
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
  void readv_callback(processed_data_vecs read_metadata, uint64_t pfd, uint64_t task_id = -1) override;
  void event_callback(int pfd, int op_res_num, uint64_t additional_info = -1) override;
  void shutdown_callback(int how, uint64_t pfd, int op_res_num, uint64_t task_id = -1) override;
  void close_callback(uint64_t pfd, int op_res_num, uint64_t task_id = -1) override;

private:
  // need to deal with various types operation_types
  void application_close_callback(int pfd, uint64_t task_id = -1);
  void close_pfd_gracefully(int pfd, uint64_t task_id = -1); // will call shutdown if needed

  // helper methods
  bool http_response_method(int pfd, buff_data data = {}, bool failed_req = false);
  bool websocket_frame_response_method(int pfd, buff_data data = {}, bool failed_req = false);
  void network_read_procedure(int pfd, uint64_t task_id, bool failed_req = false, buff_data data = {});

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
                     const http_request &req) {
    size_t file_num = local_open(filepath, O_RDONLY);
    bool file_not_found = false;

    if (static_cast<int64_t>(file_num) < 0) {
      file_not_found = true;
      file_num = local_open(not_found_filepath, O_RDONLY);

      if (static_cast<int64_t>(file_num) < 0) {
        std::cerr << "Error file not found\n";
        return file_num;
      }
    }

    std::cout << "file pfd: " << file_num << "\n";

    struct stat sb {};
    local_fstat(file_num, &sb);
    const size_t size = sb.st_size;
    uint8_t *buff = (uint8_t *)MALLOC(size);

    auto file_task_id = get_task(operation_type::HTTP_SEND_FILE, buff, size);
    auto &task = task_data[file_task_id];
    task.for_client_num = client_num;

    if (!file_not_found) {
      task.write_ranges = req.get_ranges(size);
      task.filepath = filepath;
    } else {
      task.filepath = not_found_filepath;
    }

    std::cout << "task filepath: " << task.filepath << ", file size: " << size << "\n";

    return ev->submit_read(file_num, buff, size, file_task_id);
  }

  // same as normal read but carries info about what connection type
  int raw_read(int client_num, buff_data data);
  int raw_readv(int client_num, struct iovec *iovs, size_t num_iovecs);
  int raw_write(int client_num, buff_data data);
  int raw_writev(int client_num, struct iovec *iovs, size_t num_iovecs);
  int raw_close(int client_num);

  int eventfd_open();
  int eventfd_trigger(int client_num);
  int eventfd_prepare(int client_num, uint64_t additional_info);

  int local_open(const char *pathname, int flags);
  int local_stat(const char *pathname, struct stat *buff);
  int local_fstat(int client_num, struct stat *buff);
  int local_unlink(const char *pathname);

  void start();
};

#endif