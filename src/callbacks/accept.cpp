#include "header/debug_mem_ops.hpp"
#include "header/utility.hpp"
#include "network_server.hpp"

void network_server::accept_callback(int listener_pfd, sockaddr_storage *user_data, socklen_t size,
                                     uint64_t pfd, int op_res_num, uint64_t additional_info) {
  // clears the queue, so previous queued stuff doesn't interfere with the accept stuff
  ev->submit_all_queued_sqes();

  if (op_res_num < 0) {
    switch (errno) {
    // for these errors, just try again, otherwise fail
    case ECONNABORTED:
    case EINTR:
      PRINT_DEBUG("EINTR/ECONNABORTED failed: resubmitting accept");
      if (ev->submit_accept(listener_pfd) < 0) {
        // for now just exits if the accept failed
        PRINT_ERROR_DEBUG("\t\tEINTR resubmit failed:");
        PRINT_DEBUG_VARS(op_res_num, pfd, listener_pfd, errno);
        FATAL_ERROR("Accept EINTR resubmit failed");
      }
      break;
    default:
      // there was some other error, in case of accept treat it as fatal for now
      // but only if the server is still alive
      if(!ev->is_dying_or_dead()) {
        PRINT_DEBUG("Something else failed:");
        PRINT_DEBUG_VARS(op_res_num, pfd, listener_pfd);
        FATAL_ERROR_VARS(__FUNCTION__, __LINE__, errno, op_res_num);
      }

      // in case of any of these errors, just close the socket
      ev->close_pfd(pfd);
    }
    return;
  }

  // also read from the user
  auto buff = (uint8_t *)MALLOC(READ_SIZE);
  auto user_task_id = get_task_buff_op(operation_type::NETWORK_READ, buff, READ_SIZE);
  auto &task = task_data[user_task_id];

  // submits the read, passes the task id as additional info
  // doesn't use queue_read because if this read fails, then submit will fail too
  if (ev->submit_read(pfd, task.buff, task.buff_length, user_task_id) < 0) {
    auto listener_pfd_data = ev->get_pfd_data(listener_pfd);
    auto pfd_data = ev->get_pfd_data(pfd);
    PRINT_ERROR_DEBUG("\tinitial read failed:");
    PRINT_DEBUG_VARS(errno, listener_pfd, listener_pfd_data.fd, pfd, pfd_data.fd);

    // if the queueing operation didn't work, end this connection
    FREE(buff);
    free_task(user_task_id);
    ev->close_pfd(pfd);
  }

  // carry on listening, submits everything in the queue with it, not using task_id for this
  if (ev->submit_accept(listener_pfd) < 0 && !ev->is_dying_or_dead()) {
    auto listener_pfd_data = ev->get_pfd_data(listener_pfd);
    PRINT_ERROR_DEBUG("\t\ttsubmit failed:");
    PRINT_DEBUG_VARS(errno, listener_pfd, listener_pfd_data.fd);
    FATAL_ERROR("Submit accept normal resubmit failed");
  }
}