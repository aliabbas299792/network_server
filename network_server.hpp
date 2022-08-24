#ifndef NETWORK_SERVER
#define NETWORK_SERVER

#include "subprojects/event_manager/event_manager.hpp"
#include "utility.hpp"
#include <sstream>
#include <string>

class network_server;

struct read_data {};

struct write_data {};

// the term client num is synonymous with pfd (pseudo fd) for the application stuff
class application_methods {
public:
  virtual void accept_callback(network_server *ns, int client_num) {}
  virtual void read_callback(network_server *ns, read_data data, int client_num) {}
  virtual void write_callback(network_server *ns, write_data data, int client_num) {}
  virtual void event_callback(network_server *ns, uint64_t additional_info, int client_num) {}
  virtual void close_callback(network_server *ns, int client_num) {}

  virtual ~application_methods();
};

class network_server : public server_methods {
private:
  application_methods *callbacks{}; // callbacks for the application

  event_manager ev{};
  int listener_pfd{};

  void accept_callback(event_manager *ev, int listener_pfd, sockaddr_storage *user_data, socklen_t size, uint64_t pfd, int op_res_num) override;
  void read_callback(event_manager *ev, processed_data read_metadata, uint64_t pfd) override;
  void write_callback(event_manager *ev, processed_data write_metadata, uint64_t pfd) override;
  void event_callback(event_manager *ev, uint64_t additional_info, int pfd, int op_res_num) override;
  void shutdown_callback(event_manager *ev, int how, uint64_t pfd, int op_res_num) override;
  void close_callback(event_manager *ev, uint64_t pfd, int op_res_num) override;

public:
  network_server(int port, application_methods *callbacks) {
    if(callbacks == nullptr) {
      std::string error = "Application methods callbacks must be set (" + std::string(__FUNCTION__);
      error += ": " + std::to_string(__LINE__);
      utility::fatal_error(error);
    }

    ev.set_server_methods(this);
    
    this->callbacks = callbacks;

    listener_pfd = utility::setup_listener_pfd(port, &ev);
    ev.submit_accept(listener_pfd);

    ev.start();
  }
};

#endif