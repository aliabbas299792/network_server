deps = [libnetwork_server_dep, libevent_manager_dep]

general_test = executable('general_test', 'tests.cpp', dependencies: deps, include_directories: incdir)
cache_test = executable('cache_test', 'lru_cache_tests.cpp', dependencies: deps, include_directories: incdir)
http_req_test = executable('http_req_test', 'http_request_tests.cpp', dependencies: deps, include_directories: incdir)

test('LRU cache test', cache_test)
test('HTTP Request Test', http_req_test)
test('General test', general_test)