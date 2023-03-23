project('network_server', 'cpp', default_options: ['cpp_std=c++2a'])

libevent_manager_proj = subproject('event_manager')
libevent_manager_dep = libevent_manager_proj.get_variable('libevent_manager_dep')

cpp_args_arr = ['-DVERBOSE_DEBUG', '-DFD_MEMORY_CACHE_STATS']

src_files = ['src/utility.cpp', 'src/core.cpp', 'src/http_request.cpp', 'src/http.cpp',
  'src/http_send_file.cpp', 'src/websocket.cpp', 'src/debug_mem_ops.cpp',
  'src/raw_ops.cpp', 'src/http_response.cpp', 'src/lru.cpp',
  'src/callbacks/accept.cpp', 'src/callbacks/close.cpp', 'src/callbacks/common.cpp',
  'src/callbacks/event.cpp', 'src/callbacks/read.cpp', 'src/callbacks/readv.cpp',
  'src/callbacks/write.cpp', 'src/callbacks/writev.cpp'
]

incdir = [include_directories('./')]

libnetwork_server = library('network_server', src_files, dependencies: [libevent_manager_dep], include_directories : incdir, cpp_args: cpp_args_arr)
libnetwork_server_dep = declare_dependency(link_with: libnetwork_server)

deps = [libnetwork_server_dep, libevent_manager_dep]

subdir('tests')
subdir('example')
