project('network_server', 'cpp', default_options: ['cpp_std=c++2a'])

libevent_manager_proj = subproject('event_manager')
libevent_manager_dep = libevent_manager_proj.get_variable('libevent_manager_dep')

src_files = ['src/utility.cpp', 'src/callbacks.cpp', 
  'src/core.cpp', 'src/http_request.cpp', 'src/http.cpp',
  'src/http_send_file.cpp', 'src/websocket.cpp', 'src/debug_mem_ops.cpp',
  'src/raw_ops.cpp', 'src/http_response.cpp', 'src/lru.cpp'
]

libnetwork_server = library('network_server', src_files, dependencies: [libevent_manager_dep])
libnetwork_server_dep = declare_dependency(link_with: libnetwork_server)

subdir('tests')
subdir('example')