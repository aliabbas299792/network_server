#include "../header/http_response.h"
#include "header/metadata.hpp"
#include "network_server.hpp"
#include <cstring>

bool network_server::http_response_method(int pfd, bool &auto_resubmit_read, buff_data data,
                                          bool failed_req) {
  // auto resubmit is true by default

  http_request req{reinterpret_cast<char *>(data.buffer), data.size};

  std::cout << "got a request: " << req.req_type << ", of size: " << data.size << " from fd "
            << ev->get_pfd_data(pfd).fd << " with id " << ev->get_pfd_data(pfd).id << "\n";

  if (req.req_type == "POST") {
    if (req.content_length) {
      size_t content_len = std::atoi(req.content_length);
      if (req.content) {
        size_t acutal_content_len = strlen(req.content);
        if (acutal_content_len == content_len) {
          // full body already sent
          if (data.size != READ_SIZE) {
            // if the buffer is bigger than the usual one, resubmit using a smaller one
            // the old buffer will be taken care of when using this flag
            auto_resubmit_read = false;

            uint8_t *normal_buff = (uint8_t *)MALLOC(READ_SIZE);
            auto net_read_task_id = get_task(operation_type::NETWORK_READ, normal_buff, READ_SIZE);
            ev->submit_read(pfd, normal_buff, READ_SIZE, net_read_task_id);
            std::cout << "back to normal read...\n";
          }
        } else if (acutal_content_len < content_len) {
          // need to send another read request to get the full body
          auto_resubmit_read = false; // we will submit a custom sized read request
          // below will contain the entire buffer
          size_t large_buff_size = (data.size - acutal_content_len) + content_len;
          std::cout << "allocating buffer of size: " << large_buff_size
                    << ", and expecting this many more bytes: " << (large_buff_size - data.size) << "\n";
          uint8_t *large_buff = (uint8_t *)MALLOC(large_buff_size);
          memcpy(large_buff, data.buffer, data.size);

          auto read_post_task_id = get_task(operation_type::HTTP_POST_READ, large_buff, large_buff_size);
          auto &read_task = task_data[read_post_task_id];
          read_task.progress = data.size;
          ev->submit_read(pfd, &large_buff[data.size], large_buff_size - data.size, read_post_task_id);
          std::cout << "posting more data...\n";
          return true;
        }
      } else if (content_len != 0) {
        // has no body but content length isn't 0, close connection
        return false;
      }
    } else {
      // invalid post request, close the connection
      return false;
    }
  }

  if (req.valid_req) {
    callbacks->http_read_callback(std::move(req), pfd, failed_req);
    return true;
  }

  return false;
}

int network_server::http_writev(int pfd, struct iovec *iovs, size_t num_iovecs) {
  auto task_id = get_task(operation_type::HTTP_WRITEV, iovs, num_iovecs);
  return ev->submit_writev(pfd, iovs, num_iovecs, task_id);
}

// read not reading since this is auto submitted
int network_server::http_write(int pfd, char *buff, size_t buff_length) {
  auto buff_ptr = reinterpret_cast<uint8_t *>(buff);
  auto task_id = get_task(operation_type::HTTP_WRITE, buff_ptr, buff_length);
  return ev->submit_write(pfd, buff_ptr, buff_length, task_id);
}

int network_server::http_close(int pfd) {
  auto task_id = get_task();
  auto &task = task_data[task_id];
  task.op_type = operation_type::HTTP_CLOSE;

  close_pfd_gracefully(pfd, task_id);
  return 0;
}