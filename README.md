# Network Server
The network server should be able to load in the `event_manager` library dynamically, instantiate it, start it, and replace an older version with a newer version.
Similarly the application stuff should be able to do the same for the `network_server`.
This is just a thought for later.

# Todo
- websocket negotiation
- websocket unpacking frames
- websocket packing frames
- websocket broadcasts, add another task_type which is basically reference counter
  - and find out how to constrain ranges container to containers storing ints

- test accept_bytes stuff with music/video
- add in support for POSTing large amounts of data

- add in TLS support

# Bugs
- Memory leak due to user stuff, maybe put iovec/buffer stuff in close callback to allow user to free the stuff
- Look into SIGPIPE stuff
- Submit randomly failed, I was unable to reproduce this (it was around the "Submit accept normal resubmit failed" call)