#include "header/http_request.hpp"
#include <chrono>
#include <fcntl.h>
#include <fstream>
#include <cstdio>
#include <iostream>
#include <sys/inotify.h>
#include <thread>

#include "tests.hpp"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "vendor/doctest/doctest/doctest.h"

std::string test_response = "GET /favicon.ico HTTP/1.1\r\n"
  "Host: erewhon.chickenkiller.com:4001\r\n"
  "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:107.0) Gecko/20100101 Firefox/107.0\r\n"
  "Accept: image/avif,image/webp,*/*\r\n"
  "Accept-Language: en-GB,en;q=0.5\r\n"
  "Accept-Encoding: gzip, deflate\r\n"
  "Connection: keep-alive\r\n"
  "Referer: http://erewhon.chickenkiller.com:4001/a.html\r\n"
  "Pragma: no-cache\r\n"
  "Cache-Control: no-cache\r\n\r\n";

char *make_response() {
  return malloc_str(test_response);
}

TEST_CASE("HTTP Request tests") {
  char *resp = make_response();
  http_request req{resp, test_response.length()};

  FREE(resp);
}