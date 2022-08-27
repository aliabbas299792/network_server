project('network_server', 'cpp', default_options: ['cpp_std=c++2a'])

libevent_manager_proj = subproject('event_manager')
libevent_manager_dep = libevent_manager_proj.get_variable('libevent_manager_dep')

executable('test_proj', [
    'main.cpp', 'utility.cpp', 'callbacks.cpp', 'core.cpp', 'http.cpp', 'websocket.cpp', 'intermediary_methods.cpp'
], dependencies: [libevent_manager_dep])