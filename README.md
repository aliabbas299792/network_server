# Network Server
The network server should be able to load in the `event_manager` library dynamically, instantiate it, start it, and replace an older version with a newer version.
Similarly the application stuff should be able to do the same for the `network_server`.
This is just a thought for later.

## Todo now
Have an `additional_info` parameter for all the event manager calls, which will be used here to store a `task_id`, which will index into an array to access any relevant info about the current read/write or whatever task.
Need to have some way to store/free task IDs (probably the same approach as in event manager with the pfds, with freed pfds and vector of pfd data).