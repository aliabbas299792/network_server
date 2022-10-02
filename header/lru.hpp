#ifndef LRU_HPP
#define LRU_HPP

#include <cstddef>
#include <string>

struct item_data {
  char *buff{};
  size_t buff_length{};
  std::string item_name{};
};

struct item_node {
  struct item_node *next{};
  struct item_node *prev{};
  size_t num_locks{};

  item_data data{};
};

class LRU {
  item_node *head{};
  item_node *tail{};
  size_t num_items{};

  size_t max_num_items{};

  item_node *get_item(std::string item_name);

public:
  // it is assumed the buffer is to be managed by the LRU from here
  const bool add_item(std::string item_name, char *buff, size_t buff_length);
  bool remove_item(std::string item_name);
  item_data *const get_and_lock_item(std::string item_name);
  void unlock_item(std::string item_name);

  LRU(size_t size);
};

#endif