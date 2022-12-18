#include "header/http_response.h"
#include "network_server.hpp"
#include "header/lru.hpp"
#include "subprojects/event_manager/event_manager.hpp"

void network_server::http_send_file_writev_submit(uint64_t pfd, uint64_t task_id) {
  // http send file, read stage succeeded
  auto &task = task_data[task_id];
  task.additional_ptr = task.buff;
  auto client_pfd = task.for_client_num;
  task.for_client_num = -1;

  const bool using_ranges =
      task.write_ranges.size() != 0 && static_cast<size_t>(task.write_ranges_idx) < task.write_ranges.size();

  http_send_file_writev_submit_helper(task_id, client_pfd, using_ranges);

  // this potentially causes a reallocation which invalidates the task reference
  raw_close(pfd); // close the file
}

int network_server::http_send_file_writev_submit_helper(int task_id, int client_pfd, bool using_ranges) {
  char *resp_data{};
  size_t resp_len{};
  auto &task = task_data[task_id];

  if (using_ranges) {
    http_response resp{http_resp_codes::RESP_206_PARTIAL, http_ver::HTTP_10,
                       task.write_ranges[task.write_ranges_idx],
                       http_response::get_type_from_filename(task.filepath), task.buff_length};
    resp_data = resp.allocate_buffer();
    resp_len = resp.length();
  } else {
    http_response resp{http_resp_codes::RESP_200_OK, http_ver::HTTP_10,
                       http_response::get_type_from_filename(task.filepath), task.buff_length};
    resp_data = resp.allocate_buffer();
    resp_len = resp.length();
  }

  iovec *vecs = (iovec *)MALLOC(sizeof(iovec) * 2);
  vecs[0].iov_base = resp_data;
  vecs[0].iov_len = resp_len;
  if (using_ranges) {
    auto &first_range = task.write_ranges[task.write_ranges_idx];
    vecs[1].iov_base = &task.buff[first_range.start];
    vecs[1].iov_len = first_range.end - first_range.start;
  } else {
    vecs[1].iov_base = task.buff;
    vecs[1].iov_len = task.buff_length;
  }

  task.write_ranges_idx++; // writing the first range

  task.buff = reinterpret_cast<uint8_t *>(vecs);
  task.iovs = (iovec *)MALLOC(sizeof(iovec) * 2);
  task.num_iovecs = 2;
  memcpy(task.iovs, task.buff, task.num_iovecs * sizeof(iovec));

  return ev->submit_writev(client_pfd, task.iovs, task.num_iovecs, task_id);
}

void network_server::http_send_file_read_failed(uint64_t pfd, uint64_t task_id) {
  auto &task = task_data[task_id];
  FREE(task.buff);
  callbacks->http_writev_callback(nullptr, 0, pfd, true);
}

void network_server::http_send_file_writev_finish(uint64_t pfd, uint64_t task_id, bool failed_req) {
  auto &task = task_data[task_id];
  auto original_iovs_ptr = reinterpret_cast<iovec *>(task.buff);
  FREE(original_iovs_ptr[0].iov_base); // frees the header
  FREE(task.buff);                     // frees the original iovec
  FREE(task.iovs);                     // frees the iovec copy

  // 'unlocks' item if it is in the cache, so that it 
  cache.unlock_item(task.filepath);
  // additional_ptr has the pointer to the buffer read
  // into memory the data that was read
  if (task.additional_ptr != nullptr) {
    // cache the item for later use
    if (true || !cache.add_item(task.filepath, reinterpret_cast<char *>(task.additional_ptr),
          task.buff_length)) {
      // couldn't add to cache
      FREE(task.additional_ptr);
    }
  }

  callbacks->http_writev_callback(nullptr, 0, pfd);
}

void network_server::http_send_file_writev_continue(uint64_t pfd, uint64_t task_id) {
  auto &task = task_data[task_id];
  auto original_iovs_ptr = reinterpret_cast<iovec *>(task.buff);

  if (task.write_ranges.size() != 0 &&
      static_cast<size_t>(task.write_ranges_idx) < task.write_ranges.size()) {
    char *resp_data{};
    size_t resp_len{};

    http_response resp{http_resp_codes::RESP_206_PARTIAL, http_ver::HTTP_10,
                       task.write_ranges[task.write_ranges_idx],
                       http_response::get_type_from_filename(task.filepath), task.buff_length};
    resp_data = resp.allocate_buffer();
    resp_len = resp.length();

    auto &write_range = task.write_ranges[task.write_ranges_idx];
    task.write_ranges_idx++; // writing the first range

    FREE(original_iovs_ptr[0].iov_base); // frees the previous header
    FREE(task.iovs);                     // frees the previous iovec copy
    // still need the buffer, the original iovec and the ranges
    // original_iovs_ptr is task.buff
    original_iovs_ptr[0].iov_base = resp_data;
    original_iovs_ptr[0].iov_len = resp_len;
    original_iovs_ptr[1].iov_base = &task.buff[write_range.start];
    original_iovs_ptr[1].iov_len = write_range.end - write_range.start;

    task.iovs = (iovec *)MALLOC(sizeof(iovec) * 2);
    memcpy(task.iovs, task.buff, task.num_iovecs * sizeof(iovec));

    ev->submit_writev(pfd, task.iovs, task.num_iovecs, task_id);
  } else {
    http_send_file_writev_finish(pfd, task_id, false);
    // successful
    close_pfd_gracefully(pfd, task_id);
  }
}

int network_server::http_send_cached_file(int client_num, std::string filepath_str) {
  item_data *file_buff_info = cache.get_and_lock_item(filepath_str);

  if(file_buff_info != NULL) {
    // item is cached, so send the cached file
    auto task_id = get_task(HTTP_SEND_FILE, 
      reinterpret_cast<uint8_t*>(file_buff_info->buff), file_buff_info->buff_length);
    return http_send_file_writev_submit_helper(task_id, client_num, false);
  }
  return -1;
}

int network_server::http_send_file(int client_num, const char *filepath, const char *not_found_filepath,
                                   const http_request &req) {
  std::string filepath_str = filepath;

  // if this file is cached, send it
  int send_cached_ret = http_send_cached_file(client_num, filepath_str);
  if(send_cached_ret >= 0) {
    return send_cached_ret;
  }

  int file_fd = open(filepath, O_RDONLY);
  bool file_not_found = false;

  if (file_fd < 0) {
    file_not_found = true;
    filepath_str = not_found_filepath;

    // if this file is cached, send it
    int send_cached_ret = http_send_cached_file(client_num, filepath_str);
    if(send_cached_ret >= 0) {
      return send_cached_ret;
    }

    file_fd = open(not_found_filepath, O_RDONLY);

    if (file_fd < 0) {
      std::cerr << "Error file not found\n";
      return file_fd;
    }
  }

  struct stat sb {};
  fstat(file_fd, &sb);
  const size_t size = sb.st_size;

  auto file_task_id = get_task();
  auto &task = task_data[file_task_id];
  task.for_client_num = client_num;
  task.buff_length = size;
  task.op_type = operation_type::HTTP_SEND_FILE;

  bool valid_range = true;
  if (!file_not_found) {
    task.write_ranges = req.get_ranges(size, &valid_range);
  }

  auto file_pfd = ev->pass_fd_to_event_manager(file_fd, false);

  if (!valid_range) {
    close_pfd_gracefully(file_pfd);
    free_task(file_task_id);

    http_response resp{http_resp_codes::RESP_416_UNSATISFIABLE_RANGE, http_ver::HTTP_10, "text/html", 0};
    auto buff = resp.allocate_buffer();
    std::cout << "buff len is: " << resp.length() << "\n";
    http_write(client_num, buff, resp.length());
    return 0;
  }

  task.buff = (uint8_t *)MALLOC(size);
  task.filepath = filepath;

  return ev->submit_read(file_pfd, task.buff, size, file_task_id);
}