# Network Server
The network server should be able to load in the `event_manager` library dynamically, instantiate it, start it, and replace an older version with a newer version.
Similarly the application stuff should be able to do the same for the `network_server`.
This is just a thought for later.

# Todo
- Need to add following functions to network server, close should close eventfd, network or file sockets correctly (use close_pfd in event_manager)
  - Generic fd ops: read, write, accept, close
  - Eventfd stuff: open_eventfd, trigger_event, prepare_event
  - Specifically file ops: open, socket, stat, fstat, unlink