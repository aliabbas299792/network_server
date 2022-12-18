#include "../header/lru.hpp"
#include "network_server.hpp"
#include <sys/inotify.h>
#include <unistd.h>

// placed what event we are using here for ease of reading
const int MODIFY_EVENT = IN_MODIFY;
const int DELETE_OR_MOVE_EVENT = IN_DELETE_SELF;

int lru_file_cache::get_inotify_fd() {
  return inotify_fd;
}

void lru_file_cache::process_inotify_event(inotify_event *e) {
  if(e != nullptr && e->mask) { // file has been modified/moved/deleted
    for (auto n = head; n != nullptr; n = n->next) {
      if (n->watch == e->wd) {
        n->outdated = true;
        remove_item(n->data.file_name);
        return;
      }
    }
  }
}

bool lru_file_cache::add_item(std::string file_name, char *buff, size_t buff_length) {
  auto node = get_item(file_name);
  if (node != nullptr) {
    if (node->num_locks == 0) {
      // if you can, then update the current contents
      FREE(node->data.buff);

      node->data.buff = buff;
      node->data.buff_length = buff_length;
      return true;
    }
    return false;
  }

  if (num_items < max_num_items) {
    auto new_node = reinterpret_cast<item_node*>(MALLOC(sizeof(item_node)));
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
    new_node->data.file_name = file_name;
    // monitor for deletion and modification
    new_node->watch = inotify_add_watch(inotify_fd, file_name.c_str(), MODIFY_EVENT | DELETE_OR_MOVE_EVENT);

    num_items++;
    return true;
  } else {
    if (tail != nullptr) {
      // try removing the last item
      // if it succeeds, then try adding again
      if (remove_item(tail->data.file_name)) {
        std::cout << "we removed item " << tail->data.file_name << "\n";
        return add_item(file_name, buff, buff_length);
      }
      std::cout << "cant do it blad cant do it.....\n";
      return false;
    }
  }
  return false;
}

item_node *lru_file_cache::get_item(std::string file_name) {
  for (auto n = head; n != nullptr; n = n->next) {
    if (n->data.file_name == file_name) {
      return n;
    }
  }
  return nullptr;
}

item_data *const lru_file_cache::get_and_lock_item(std::string file_name) {
  auto node = get_item(file_name);
  // promote to front of list
  if (node == nullptr) {
    return nullptr;
  }

  if(node->outdated) {
    remove_item(file_name);
    return nullptr;
  }

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

void lru_file_cache::unlock_item(std::string file_name) {
  for (auto n = head; n != nullptr; n = n->next) {
    if (n->data.file_name == file_name) {
      n->num_locks = std::max(0ul, n->num_locks - 1);
    }
  }
}

bool lru_file_cache::remove_item(std::string file_name) {
  auto node = get_item(file_name);

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

  inotify_rm_watch(inotify_fd, node->watch);
  FREE(node->data.buff); // frees the stored buffer
  FREE(node);            // frees the node

  return true;
}

lru_file_cache::lru_file_cache(size_t size, network_server *ns) {
  max_num_items = size;
  this->ns = ns;
  inotify_fd = inotify_init();
}

lru_file_cache::~lru_file_cache() { close(inotify_fd); }