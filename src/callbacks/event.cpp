#include "network_server.hpp"

void network_server::event_callback(int pfd, int op_res_num, uint64_t additional_info) {
  if (op_res_num < 0) {
    switch (errno) {
    case EINTR:
      // for these errors, just try again, otherwise fail
      if (ev->submit_generic_event(pfd, additional_info) < 0) {
        callbacks->event_trigger_callback(pfd, additional_info, true);
      }
      break;
    default: {
      // in case of any of these errors, just close the socket
      ev->close_pfd(pfd, additional_info);

      // there was some other error
      callbacks->event_trigger_callback(pfd, additional_info, true);
      return;
    }
    }
  }

  callbacks->event_trigger_callback(pfd, additional_info);
}