#include "network_server.hpp"

network_server::network_server(int port, application_methods *callbacks) {
  if (callbacks == nullptr) {
    std::string error = "Application methods callbacks must be set (" + std::string(__FUNCTION__);
    error += ": " + std::to_string(__LINE__);
    utility::fatal_error(error);
  }

  ev->set_server_methods(this);

  this->callbacks = callbacks;

  listener_pfd = utility::setup_listener_pfd(port, ev);
  ev->submit_accept(listener_pfd);

  ev->start();
}

int network_server::get_task() {
  int id = 1;

  if (task_freed_idxs.size() > 0) {
    id += *task_freed_idxs.begin();
    task_freed_idxs.erase(id);

    auto &freed_task = task_data[id];
    freed_task = task();
  } else {
    task_data.emplace_back();
    id += task_data.size() - 1;
  }

  return id;
}

int network_server::get_task(uint8_t *buff, size_t length) {
  auto id = get_task();
  auto &task = task_data[id];
  task.buff = buff;
  task.buff_length = length;

  return id;
}

void network_server::free_task(int task_id) { task_freed_idxs.insert(task_id); }