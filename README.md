# Network Server
A simple library which can be used to write a web server.

# Usage
- For all methods with `bool failed_req = false` as the final parameter, this basically indicates this event failed, so use this to handle clean up operations
- A `client_num` refers to a socket or any sort of file descriptor that is being used
## Methods
### Event
- First you make a new `eventfd` with `eventfd_open()`
- Then you prepare an event with some information using `eventfd_prepare(pfd, additional_info)`
- Then you trigger it, and the callback will be called using `eventfd_trigger(pfd)`

### Raw
- Used with any `client_num`, same as normal `read`, `write`, `writev`, `reav` and `close` calls

### Local
- Used to do non-async operations, `local_open` (same as `open`), `local_stat` (same as `stat`), `local_fstat` (same as `fstat`) and `local_unlink` (same as `unlink`)
- Use only these functions with the network server since they operate on/use `client_nun`s where necessary

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
- add in LRU cache or something similar
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