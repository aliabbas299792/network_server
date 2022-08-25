#include "network_server.hpp"
#include "metadata.hpp"

network_server::network_server(int port, application_methods *callbacks) {
  if (callbacks == nullptr) {
    std::string error = "Application methods callbacks must be set (" + std::string(__FUNCTION__);
    error += ": " + std::to_string(__LINE__);
    utility::fatal_error(error);
  }

  ev.set_server_methods(this);

  this->callbacks = callbacks;

  listener_pfd = utility::setup_listener_pfd(port, &ev);
  ev.submit_accept(listener_pfd);

  ev.start();
}

void network_server::accept_callback(event_manager *ev, int listener_fd, sockaddr_storage *user_data,
                                     socklen_t size, uint64_t pfd, int op_res_num) {
  if (op_res_num < 0) {
    switch (errno) {
    // for these errors, just try again, otherwise fail
    case ECONNABORTED:
    case EINTR:
      ev->submit_accept(pfd);
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

    return;
  }

  callbacks->accept_callback(this, pfd);

  // carry on listening
  ev->submit_accept(listener_fd);

  // also read from the user
  if (pfd >= clients_data.size()) {
    clients_data.resize(pfd + 1); // fit all client data in here
  }

  auto &client_info = clients_data[pfd];
  client_info.op_type = operation_type::NETWORK_READ;
}

void network_server::read_callback(event_manager *ev, processed_data read_metadata, uint64_t pfd) {
  if (read_metadata.op_res_num < 0) {
    switch (errno) {
    // for these errors, just try again, otherwise fail
    case EINTR:
      ev->submit_read(pfd, read_metadata.buff, read_metadata.length, read_metadata.amount_processed_before);
      break;
    default: {
      // in case of any of these errors, just close the socket
      ev->shutdown_and_close_normally(pfd);

      // there was some other error
      callbacks->close_callback(this, pfd);
    }
    }

    return;
  }

  if (read_metadata.op_res_num == 0) { // read was 0, socket connection closed
    auto shutdown_code = ev->submit_shutdown(pfd, SHUT_WR);

    if (shutdown_code < 0) {                // shutdown procedure failed
      ev->shutdown_and_close_normally(pfd); // use normal shutdown()/close()
      close_callback(ev, pfd, 0);           // clean up resources in network server/application
    }

    return;
  }

  size_t read_this_time = read_metadata.op_res_num; // dealt with case that this is < 0 now
  size_t amount_read = read_metadata.amount_processed_before + read_this_time;

  // populate the data struct with necessary and useful info
  read_data data{amount_read, read_this_time, read_metadata.buff};

  callbacks->read_callback(this, data, pfd);
}

void network_server::write_callback(event_manager *ev, processed_data write_metadata, uint64_t pfd) {
  if (write_metadata.op_res_num < 0) {
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
  if (op_res_num < 0) {
    // in case of any of these errors, just close the socket
    ev->shutdown_and_close_normally(pfd);

    // there was some other error
    callbacks->close_callback(this, pfd);
    return;
  }

  // shutdown code
}

void network_server::close_callback(event_manager *ev, uint64_t pfd, int op_res_num) {
  if (op_res_num < 0) {
    // in case of any of these errors, just close the socket
    ev->shutdown_and_close_normally(pfd);

    // there was some other error
    callbacks->close_callback(this, pfd);
    return;
  }
}

void network_server::event_callback(event_manager *ev, uint64_t additional_info, int pfd, int op_res_num) {
  if (op_res_num < 0) {
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