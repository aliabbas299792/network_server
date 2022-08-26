# Network Server
The network server should be able to load in the `event_manager` library dynamically, instantiate it, start it, and replace an older version with a newer version.
Similarly the application stuff should be able to do the same for the `network_server`.
This is just a thought for later.

# Todo
- Need to add following functions to network server, close should close eventfd, network or file sockets correctly (use close_pfd in event_manager)
  - Generic fd ops: read (raw - async), write (http, websocket, raw - async), close (http, websocket, raw - async)
  - Eventfd stuff: open_eventfd, trigger_event, prepare_event (async)
  - Specifically file ops: open, socket, stat, fstat, unlink

So the async ones are 1 read operations, 3 write operations, 3 close operations and 1 eventfd operation.
3 read callbacks (only 2 read ops since http/websocket reading is managed by the network_server), 3 write callbacks, 3 close callbacks, 1 eventfd read callback and 1 accept callback.
- 11 callbacks in all

Rewrite network_server_callbacks.cpp to work with the 11 new `operation_type`s in network_server.hpp
- If operation type is NONE then it means that the application stuff hasn't gotten any info on this pfd yet so you can just cleanup here and return