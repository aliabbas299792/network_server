#include "network_server.hpp"
#include "subprojects/event_manager/event_manager.hpp"
#include <asm-generic/errno.h>
#include <string>

void network_server::accept_callback(event_manager *ev, int listener_fd, sockaddr_storage *user_data, socklen_t size, uint64_t pfd, int op_res_num) {
  if(op_res_num < 0) {
    switch (errno) {
      // for these errors, just try again, otherwise fail
      case ECONNABORTED:
      case EINTR:
        break;
      default: {
        // in case of any of these errors, just close the socket
        ev->shutdown_and_close_normally(pfd);

        // there was some other error, in case of accept treat it as fatal for now
        std::string error = "(" + std::string(__FUNCTION__) + ": " + std::to_string(__LINE__);
        error += "), errno: " + std::to_string(errno) + ", op_res_num: " + std::to_string(op_res_num);
        utility::fatal_error(error);
      }
    }
  }

  // accept code
}

void network_server::read_callback(event_manager *ev, processed_data read_metadata, uint64_t pfd) {
  if(read_metadata.op_res_num < 0) {
    switch (errno) {
      // for these errors, just try again, otherwise fail
      case EINTR:
        break;
      default: {
        // in case of any of these errors, just close the socket
        ev->shutdown_and_close_normally(pfd);

        // there was some other error
        callbacks->close_callback(this, pfd);
        return;
      }
    }
  }

  // read code
}

void network_server::write_callback(event_manager *ev, processed_data write_metadata, uint64_t pfd) {
  if(write_metadata.op_res_num < 0) {
    switch (errno) {
      // for these errors, just try again, otherwise fail
      case EINTR:
        break;
      default: {
        // in case of any of these errors, just close the socket
        ev->shutdown_and_close_normally(pfd);

        // there was some other error
        callbacks->close_callback(this, pfd);
        return;
      }
    }
  }

  // write code
}

void network_server::shutdown_callback(event_manager *ev, int how, uint64_t pfd, int op_res_num) {
  if(op_res_num < 0) {
    // in case of any of these errors, just close the socket
    ev->shutdown_and_close_normally(pfd);
    

    // there was some other error
    callbacks->close_callback(this, pfd);
    return;
  }

  // shutdown code
}

void network_server::close_callback(event_manager *ev, uint64_t pfd, int op_res_num) {
  if(op_res_num < 0) {
    // in case of any of these errors, just close the socket
    ev->shutdown_and_close_normally(pfd);

    // there was some other error
    callbacks->close_callback(this, pfd);
    return;
  }

}

void network_server::event_callback(event_manager *ev, uint64_t additional_info, int pfd, int op_res_num) {
  if(op_res_num < 0) {
    switch (errno) {
      // for these errors, just try again, otherwise fail
      case EINTR:
        break;
      default: {
        // in case of any of these errors, just close the socket
        ev->shutdown_and_close_normally(pfd);

        // there was some other error
        callbacks->close_callback(this, pfd);
        return;
      }
    }
  }

  // event code
}