# Network Server
The network server should be able to load in the `event_manager` library dynamically, instantiate it, start it, and replace an older version with a newer version.
Similarly the application stuff should be able to do the same for the `network_server`.
This is just a thought for later.

# Todo
- Need to add following functions to network server, close should close eventfd, network or file sockets correctly (use close_pfd in event_manager)
  - Generic fd ops: read (raw - async), write (http, websocket, raw - async), close (http, websocket, raw - async)
  - Eventfd stuff: open_eventfd, trigger_event, prepare_event (async)
  - Specifically file ops: open, stat, fstat, unlink

So the async ones are 1 read operations, 3 write operations, 3 close operations and 1 eventfd operation.
3 read callbacks (only 2 read ops since http/websocket reading is managed by the network_server), 3 write callbacks, 3 close callbacks, 1 eventfd read callback and 1 accept callback.
- 11 callbacks in all

Rewrite network_server_callbacks.cpp to work with the 11 new `operation_type`s in network_server.hpp
- If operation type is NONE then it means that the application stuff hasn't gotten any info on this pfd yet so you can just cleanup here and return












-> accept_cb
  -> error (just die or resubmit if EINTR or ECONNABORTED)
  -> read and resubmit accept (read op type NONE)

-> read_cb
  -> error
  -> for NONE -> initialise to WEBSOCKET, HTTP or RAW, then accept callback
  -> for RAW or LOCAL_READ
    -> do full/continued read procedure
  -> for HTTP or WEBSOCKET
    -> do NETWORK read procedure, pass on to appropriate procedure helper

-> write_cb
  -> error (count NONE as error, close immediately, NONE shouldn't be here)
  -> for RAW, WEBSOCKET or HTTP
    -> do the full write
    -> call the correct write callback afterwards

-> event_cb
  -> error (special error close callback since you can't close it normally)
  -> event trigger callback

-> shutdown_cb (only WEBSOCKET or HTTP can be here anything else is error)
  -> error
  -> WEBSOCKET or HTTP
    -> SHUT_RDWR or SHUT_WR -> submit close

-> close_cb
  -> error
  -> RAW, WEBSOCKET or HTTP call the correct callback



- for websocket_broadcast, add another task_type which is basically reference counter
  - and find out how to constrain ranges container to containers storing ints
- for websocket_write/http_write, add in writev and maybe readv into event_manager