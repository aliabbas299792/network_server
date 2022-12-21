# Network Server
A simple library which can be used to write a web server.

# Usage
- For all methods with `bool failed_req = false` as the final parameter, this basically indicates this event failed, so use this to handle clean up operations
- A `client_num` refers to a socket or any sort of file descriptor that is being used
- To use network server methods with your own fd, use the pass to network server method to get a clinet number to work with

## Methods
### Event
- First you make a new `eventfd` with `eventfd_open()`
- Then you prepare an event with some information using `eventfd_prepare(pfd, additional_info)`
- Then you trigger it, and the callback will be called using `eventfd_trigger(pfd)`

### Raw
- Used with any `client_num`, same as normal `read`, `write`, `writev`, `reav` and `close` calls

### HTTP
- `http_write` and `http_writev` for writing to a HTTP client, and `http_close` for closing when needed
- `http_send_file`, is a special request, regardless of error outcome, it will always give a response in the `http_writev_callback`, it basically tries sending a file to the client. You pass the http request object to it, which it will use to send the appropriate file range.
- The enum `HTTP_POST_READ` is specifically used for reading POST data

### Websocket
- `websocket_write`, `websocket_writev` for writing to a websocket client and `websocket_close` for closing a websocket client
- `websocket_broadcast` to broadcast to all `client_num`s in the passed container
## Callbacks
### Accept and event callbacks
- `void accept_callback(int client_num)`:
  - Called when a new client is accepted
- `void event_trigger_callback(int client_num, uint64_t additional_info)`:
  - Called when a custom event is triggered, the second parameter contains information you passed when you called `eventfd_prepare(...)`
- `void event_error_close_callback(int client_num, uint64_t additional_info)`:
  - Called in the off chance there is an error with after you called `eventfd_prepare(...)`, so do any cleanup required here

### Raw callbacks
- Raw, as in, not HTTP or websocket opened by the network server, they could still be a network socket of some sort, but you have to manage it
- `void raw_read_callback(buff_data data, int client_num, bool failed_req = false)`:
  - Called after something has been read from a client
- `void raw_readv_callback(struct iovec *data, size_t num_iovecs, int client_num, bool failed_req = false)`:
  - Called after something was read using `readv`, `iovecs` contain the required data
- `void raw_write_callback(buff_data data, int client_num, bool failed_req = false)`:
  - Called when something is written
- `void raw_writev_callback(struct iovec *data, size_t num_iovecs, int client_num, bool failed_req = false)`:
  - Called when something is written using `writev`
- `void raw_close_callback(int client_num)`:
  - Called when the client is closed, deal with cleanup

### Websocket callbacks
- `void websocket_read_callback(buff_data data, int client_num, bool failed_req = false)`:
  - Something read from a websocket
- `void websocket_write_callback(buff_data data, int client_num, bool failed_req = false)`:
  - Something was written to a websocket
- `void websocket_writev_callback(struct iovec *data, size_t num_iovecs, int client_num, bool failed_req = false)`:
  - Something was written to a websocket using `writev`
- `void websocket_close_callback(int client_num)`:
  - A websocket was closed, deal with cleanup

### HTTP callbacks
- `void http_read_callback(http_request req, int client_num, bool failed_req = false)`:
  - Something was read from a HTTP connected client
- `void http_write_callback(buff_data data, int client_num, bool failed_req = false)`:
  - Something was written to a HTTP connected client (connection will be closed after this)
- `void http_writev_callback(struct iovec *data, size_t num_iovecs, int client_num, bool failed_req = false)`:
  - Something was written using `writev` to a HTTP client
- `void http_close_callback(int client_num)`:
  - The client was closed, deal with cleanup

# Todo
- most importantly, the recent crashes seem to stem from error 24 - too many open files, maybe we're leaking file descriptors
- fix partial ranges on cached items
- fix LRU cache, enable it by going to http_send_file.cpp:81
  - appears to be issue with unlocking items, and potentially a memory leak
- fix freeing task IDs, since can only seem to avoid errors by not enabling freeing them
- add in unit test for rest of network server, aim for >=80% branch coverage and ~100% line coverage
- diagnose rare segmentation fault, unsure if related to cache:
```
[ Mon Dec 19 21:57:13 2022 ]: (MALLOC) current memory: 9915, malloc'd: 4096
AddressSanitizer:DEADLYSIGNAL
=================================================================
==2225418==ERROR: AddressSanitizer: SEGV on unknown address 0x7f49d0832000 (pc 0x7f49d15717dd bp 0x7ffc5eb5d250 sp 0x7ffc5eb5d220 T0)
==2225418==The signal is caused by a WRITE memory access.
    #0 0x7f49d15717dd in io_uring_prep_rw /usr/include/liburing.h:257
    #1 0x7f49d1571a87 in io_uring_prep_read /usr/include/liburing.h:507
    #2 0x7f49d15739e8 in event_manager::queue_read(int, unsigned char*, unsigned long, unsigned long) ../subprojects/event_manager/src/event_manager.cpp:224
    #3 0x7f49d157290d in event_manager::submit_read(int, unsigned char*, unsigned long, unsigned long) ../subprojects/event_manager/src/event_manager.cpp:77
    #4 0x7f49d08e0edb in network_server::accept_callback(int, sockaddr_storage*, unsigned int, unsigned long, int, unsigned long) ../src/callbacks.cpp:48
    #5 0x7f49d15780ab in event_manager::event_handler(int, request_data*) ../subprojects/event_manager/src/event_manager.cpp:621
    #6 0x7f49d15767e3 in event_manager::await_single_message() ../subprojects/event_manager/src/event_manager.cpp:501
    #7 0x7f49d157608e in event_manager::start() ../subprojects/event_manager/src/event_manager.cpp:461
    #8 0x7f49d08eded4 in network_server::start() ../src/core.cpp:39
    #9 0x55d76467893e in app_methods::app_methods(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&) ../example/example.hpp:43
    #10 0x55d76467679a in main ../example/example.cpp:108
    #11 0x7f49d0229d8f in __libc_start_call_main ../sysdeps/nptl/libc_start_call_main.h:58
    #12 0x7f49d0229e3f in __libc_start_main_impl ../csu/libc-start.c:392
    #13 0x55d764672df4 in _start (/home/watcher/network_server/build/example/network_server_example+0x2bdf4)

AddressSanitizer can not provide additional info.
SUMMARY: AddressSanitizer: SEGV /usr/include/liburing.h:257 in io_uring_prep_rw
==2225418==ABORTING
```
- add in TLS support
- The network server should be able to load in the `event_manager` library dynamically, instantiate it, start it, and replace an older version with a newer version.
- Similarly the application stuff should be able to do the same for the `network_server`.
  - This is just a thought for later.
- websocket negotiation
- websocket unpacking frames
- websocket packing frames
- websocket broadcasts, add another task_type which is basically reference counter
  - and find out how to constrain ranges container to containers storing ints

# Good practice
- Unless a bunch of operations MUST happen at once (i.e if a single failure means all of them should fail), use the `submit_*` functions rather than `queue_*` and then `submit_*`

# Helpful
Run gdb then run `nsdebug` to run a couple of convenient commands
```
define nsdebug
        handle SIGPIPE nostop noprint pass
        set breakpoint pending on
        shell ./compile.sh
        file build/example/network_server_example
        run
        bt
```
