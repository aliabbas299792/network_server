#ifndef LRU_HPP
#define LRU_HPP

#include <cstddef>
#include <string>
#include <sys/inotify.h>

class network_server;

struct item_data {
  char *buff{};
  size_t buff_length{};
  std::string file_name{};
};

struct item_node {
  struct item_node *next{};
  struct item_node *prev{};
  size_t num_locks{}; // used to prevent item from being removed while still in use

  item_data data{};
  int watch = -1;
  bool outdated = false;
};

class lru_file_cache {
private:
  item_node *head{};
  item_node *tail{};
  size_t num_items{};

  size_t max_num_items{};

  item_node *get_item(std::string file_name);
  bool remove_node(item_node *node);

  // for file monitoring
  int inotify_fd{};
  network_server *ns{};

public:
  // it is assumed the buffer is to be managed by the LRU once passed
  bool add_or_update_item(const std::string &file_name, char *buff, size_t buff_length);
  bool remove_item(std::string file_name);

  // will promote the item to the front of the list if it can be locked
  item_data *const get_and_lock_item(std::string file_name);

  void unlock_item(std::string file_name);

  void process_inotify_event(inotify_event *e);
  int get_inotify_fd();

  lru_file_cache(size_t size);
  ~lru_file_cache();
};

#endif