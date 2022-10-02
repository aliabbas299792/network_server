#include "../header/lru.hpp"

const bool LRU::add_item(std::string item_name, char *buff, size_t buff_length) {
  auto node = get_item(item_name);
  if (node != nullptr) {
    if (node->num_locks == 0) {
      // if you can, then update the current contents
      free(node->data.buff);

      node->data.buff = buff;
      node->data.buff_length = buff_length;
      return true;
    }
    return false;
  }

  if (num_items < max_num_items) {
    auto new_node = new item_node{};
    if (tail == nullptr) {
      head = new_node;
      tail = head;
    } else {
      // add to front of list
      new_node->next = head;
      head->prev = new_node;
      head = new_node;
    }

    new_node->data.buff = buff;
    new_node->data.buff_length = buff_length;
    new_node->data.item_name = item_name;

    num_items++;
    return true;
  } else {
    if (tail != nullptr) {
      // try removing the last item
      // if it succeeds, then try adding again
      if (remove_item(tail->data.item_name))
        add_item(item_name, buff, buff_length);
      return true;
    }
  }
  return false;
}

item_node *LRU::get_item(std::string item_name) {
  for (auto n = head; n != nullptr; n = n->next) {
    if (n->data.item_name == item_name) {
      return n;
    }
  }
  return nullptr;
}

item_data *const LRU::get_and_lock_item(std::string item_name) {
  auto node = get_item(item_name);
  // promote to front of list
  if (node == nullptr)
    return nullptr;

  node->num_locks++;
  auto data = &(node->data);

  if (node == head) // special case if head (1 node)
    return data;

  if (node == tail) {
    // special case if tail (2 nodes)
    tail = node->prev;
    tail->next = nullptr;
  } else {
    // fix connections of nodes before and after this node
    // since this case is for when there are 3 or more nodes
    node->prev->next = node->next;
    node->next->prev = node->prev;
  }

  // fix the previous and next nodes, make it into head
  node->prev = nullptr;
  node->next = head;
  head->prev = node;
  head = node;

  return data;
}

void LRU::unlock_item(std::string item_name) {
  for (auto n = head; n != nullptr; n = n->next) {
    if (n->data.item_name == item_name) {
      n->num_locks = std::max(0ul, n->num_locks - 1);
    }
  }
}

bool LRU::remove_item(std::string item_name) {
  auto node = get_item(item_name);

  if (node == nullptr)
    return true; // count not finding as succesful deletion

  if (node->num_locks != 0) {
    return false; // cannot delete it
  }

  if (node == head)
    head = head->next;
  if (node == tail)
    tail = tail->prev;

  if (node->prev)
    node->prev->next = node->next;
  if (node->next)
    node->next->prev = node->prev;

  num_items--;

  free(node->data.buff); // frees the stored buffer
  free(node);            // frees the node

  return true;
}

LRU::LRU(size_t size) { max_num_items = size; }