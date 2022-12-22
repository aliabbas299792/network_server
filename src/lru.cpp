#include "header/lru.hpp"
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
        remove_node(n);
        return;
      }
    }
  }
}

bool lru_file_cache::add_or_update_item(const std::string &file_name, char *buff, size_t buff_length) {
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
    // need to use new/delete so that destructors are called
    auto new_node = reinterpret_cast<item_node*>(new item_node);
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
      if (remove_node(tail)) {
        return add_or_update_item(file_name, buff, buff_length);
      }
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
    remove_node(node);
    return nullptr;
  }

  node->num_locks++;
#ifdef VERBOSE_DEBUG
  std::cout << "\t\t\t(locked) " << file_name << " has " << node->num_locks << " locks\n";
#endif
  auto data = &(node->data);

  if (node == head) { // special case if head (1 node)
    return data;
  }

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
#ifdef VERBOSE_DEBUG
  std::cout << "\t\t\t(attempt unlock) try to unlock: " << file_name << "\n";
#endif
  for (auto n = head; n != nullptr; n = n->next) {
    if (n->data.file_name == file_name && n->num_locks != 0) {
      n->num_locks--;
#ifdef VERBOSE_DEBUG
      std::cout << "\t\t\t(unlocked) " << file_name << " has " << n->num_locks << " locks\n";
#endif
      return;
    }
  }
#ifdef VERBOSE_DEBUG
  std::cout << "\t\t\t(failed unlock) wasn't able to unlock " << file_name << "\n";
#endif
}

bool lru_file_cache::remove_item(std::string file_name) {
  auto node = get_item(file_name);
  return remove_node(node);
}

bool lru_file_cache::remove_node(item_node *node) {
  if (node == nullptr) {
    return true; // count not finding as succesful deletion
  }

  if (node->num_locks != 0) {
    return false; // cannot delete it
  }

  if (node == head) {
    head = head->next;
  }

  if (node == tail) {
    tail = tail->prev;
  }

  if (node->prev) {
    node->prev->next = node->next;
  }

  if (node->next) {
    node->next->prev = node->prev;
  }

  num_items--;

  inotify_rm_watch(inotify_fd, node->watch);
  FREE(node->data.buff); // frees the stored buffer
  delete node;            // frees the node

  return true;
}

lru_file_cache::lru_file_cache(size_t size) {
  max_num_items = size;
  inotify_fd = inotify_init();
}

lru_file_cache::~lru_file_cache() { 
  for (auto n = head; n != nullptr;) {
    item_node *next = n->next;
    inotify_rm_watch(inotify_fd, n->watch);
    FREE(n->data.buff);
    delete n;
    n = next;
  }

  head = nullptr;
  tail = nullptr;
  num_items = 0;

  close(inotify_fd);
}