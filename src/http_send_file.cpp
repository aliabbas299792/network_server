#include "../header/http_response.h"
#include "../network_server.hpp"

void network_server::http_send_file_submit_writev(uint64_t pfd, uint64_t task_id) {
  // http send file, read stage succeeded
  auto &task = task_data[task_id];
  task.additional_ptr = task.buff;
  auto client_pfd = task.for_client_num;
  task.for_client_num = -1;

  const bool using_ranges = task.write_ranges.array_len != 0 &&
                            static_cast<size_t>(task.write_ranges_idx) < task.write_ranges.array_len;
  char *resp_data{};
  size_t resp_len{};
  if (using_ranges) {
    http_response resp{http_resp_codes::RESP_206_PARTIAL, http_ver::HTTP_10,
                       task.write_ranges.ranges_array[task.write_ranges_idx], task.file_type,
                       task.buff_length};
    resp_data = resp.allocate_buffer();
    resp_len = resp.length();
  } else {
    http_response resp{http_resp_codes::RESP_200_OK, http_ver::HTTP_10, task.file_type, task.buff_length};
    resp_data = resp.allocate_buffer();
    resp_len = resp.length();
  }

  iovec *vecs = (iovec *)MALLOC(sizeof(iovec) * 2);
  vecs[0].iov_base = resp_data;
  vecs[0].iov_len = resp_len;
  if (using_ranges) {
    auto &first_range = task.write_ranges.ranges_array[task.write_ranges_idx];
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

  // this potentially causes a reallocation which invalidates the task reference
  raw_close(pfd); // close the file

  auto &taskWrite = task_data[task_id];
  ev->submit_writev(client_pfd, taskWrite.iovs, taskWrite.num_iovecs, task_id);
}

void network_server::http_send_file_read_failed(uint64_t pfd, uint64_t task_id) {
  auto &task = task_data[task_id];
  FREE(task.write_ranges.ranges_array);
  FREE(task.buff);
  callbacks->http_writev_callback(nullptr, 0, pfd, true);
}

void network_server::http_send_file_writev_finish(uint64_t pfd, uint64_t task_id, bool failed_req) {
  auto &task = task_data[task_id];
  auto original_iovs_ptr = reinterpret_cast<iovec *>(task.buff);
  FREE(original_iovs_ptr[0].iov_base);  // frees the header
  FREE(task.buff);                      // frees the original iovec
  FREE(task.write_ranges.ranges_array); // frees the range data
  FREE(task.iovs);                      // frees the iovec copy
  FREE(task.additional_ptr);            // frees the data that was read
  callbacks->http_writev_callback(nullptr, 0, pfd);
}

void network_server::http_send_file_writev(uint64_t pfd, uint64_t task_id) {
  auto &task = task_data[task_id];
  auto original_iovs_ptr = reinterpret_cast<iovec *>(task.buff);

  if (task.write_ranges.array_len != 0 &&
      static_cast<size_t>(task.write_ranges_idx) < task.write_ranges.array_len) {
    char *resp_data{};
    size_t resp_len{};

    http_response resp{http_resp_codes::RESP_206_PARTIAL, http_ver::HTTP_10,
                       task.write_ranges.ranges_array[task.write_ranges_idx], task.file_type,
                       task.buff_length};
    resp_data = resp.allocate_buffer();
    resp_len = resp.length();

    auto &write_range = task.write_ranges.ranges_array[task.write_ranges_idx];
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

int network_server::http_send_file(int client_num, const char *filepath, const char *not_found_filepath,
                                   const http_request &req) {
  size_t file_num = local_open(filepath, O_RDONLY);
  bool file_not_found = false;
  std::string filepath_str = filepath;

  if (static_cast<int64_t>(file_num) < 0) {
    file_not_found = true;
    filepath_str = not_found_filepath;
    file_num = local_open(not_found_filepath, O_RDONLY);

    if (static_cast<int64_t>(file_num) < 0) {
      std::cerr << "Error file not found\n";
      return file_num;
    }
  }

  struct stat sb {};
  local_fstat(file_num, &sb);
  const size_t size = sb.st_size;
  uint8_t *buff = (uint8_t *)MALLOC(size);

  auto file_task_id = get_task(operation_type::HTTP_SEND_FILE, buff, size);
  auto &task = task_data[file_task_id];
  task.for_client_num = client_num;

  std::string type = "text/html";
  if (filepath_str.ends_with(".mp4")) {
    type = "video/mp4";
  } else if (filepath_str.ends_with(".jpg")) {
    type = "image/jpg";
  } else if (filepath_str.ends_with(".gif")) {
    type = "image/gif";
  }

  if (!file_not_found) {
    task.write_ranges = req.get_ranges(size);
  }

  task.file_type = type;

  std::cout << "task filetype: " << task.file_type << ", file size: " << size << "\n";

  return ev->submit_read(file_num, buff, size, file_task_id);
}