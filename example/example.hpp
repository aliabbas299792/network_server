#ifndef NETWORK_SERVER_EXAMPLE
#define NETWORK_SERVER_EXAMPLE

#include "header/debug_mem_ops.hpp"
#include <filesystem>
#include <thread>
#include <sys/resource.h>
#include <dirent.h>
constexpr int port = 4001;

#include "header/http_response.hpp"
#include "network_server.hpp"

struct job_data {
  int client_num = -1;
  std::string filepath{};
};

class app_methods : public application_methods {
private:
  int count_open_fds() {
    // https://stackoverflow.com/a/65007429/3605868
    DIR *dp = opendir("/proc/self/fd");
    struct dirent *de;
    int count = -3; // '.', '..', dp

    if (dp == NULL)
      return -1;

    while ((de = readdir(dp)) != NULL) {
      // printf("de %d: %s\n", count, de->d_name);
      count++;
    }

    (void)closedir(dp);

    return count;
  }

public:
  void raw_read_callback(buff_data data, int client_num, bool failed_req = false) override;

  void http_read_callback(http_request &&req, int client_num, bool failed_req) override;
  void http_write_callback(buff_data data, int client_num, bool failed_req) override;
  void http_writev_callback(struct iovec *data, size_t num_iovecs, int client_num, bool failed_req) override;
  void http_close_callback(int client_num) override;

  std::string base_file_path{};

  std::vector<job_data> job_requests{};

  app_methods(const std::string &base_file_path) {
    rlimit rlim{};
    getrlimit(RLIMIT_NOFILE, &rlim);
    rlim.rlim_cur = rlim.rlim_max;
    PRINT_DEBUG(setrlimit(RLIMIT_NOFILE, &rlim) << " is rlimit res");

    event_manager ev{};
    network_server ns{port, &ev, this};

    this->base_file_path = base_file_path;
    this->set_network_server(&ns);

    auto t = std::thread([&] {
      while(true) {
#ifdef FD_MEMORY_CACHE_STATS
        ns.print_cache_stats();
        MEM_PRINT();

        rlimit rlim{};
        getrlimit(RLIMIT_NOFILE, &rlim);
        PRINT_DEBUG(" ### NUM OPEN FILE DESCRIPTORS (num open/max allowed): " << count_open_fds() << "/" << rlim.rlim_cur << ", hard limit is: " << rlim.rlim_max);
#endif

        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    });

    ns.start();
  }
};

#endif