#include <cstring>
#include <iostream>

#include "utility.h"

namespace utility {

struct b64quartet {
  b64quartet(char a, char b, char c) {
    static char b64_alphabet[] = {
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
        'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
        'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
        'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};
    str[0] = b64_alphabet[(a & 0b11111100) >> 2];
    str[1] = b64_alphabet[(a & 0b00000011) << 4 | (b & 0b11110000) >> 4];
    str[2] = b64_alphabet[(b & 0b00001111) << 2 | (c & 0b11000000) >> 6];
    str[3] = b64_alphabet[(c & 0b00111111)];
    str[4] = 0;
  }
  char str[5];
};

std::string b64_encode(const char *str) {
  int len = strlen(str);

  std::string b64_str{};

  for (int i = 0; i < len; i += 3) {
    char a = str[i];
    char b = i + 1 < len ? str[i + 1] : 0;
    char c = i + 2 < len ? str[i + 2] : 0;

    b64_str += b64quartet(a, b, c).str;
  }

  int b64_str_len = b64_str.size();
  int num_padding = (3 - len % 3) % 3;
  for (int i = b64_str_len - num_padding; i < b64_str_len; i++) {
    b64_str[i] = '=';
  }

  return b64_str;
}

void log_helper_function(std::string msg, bool cerr_or_not) {
  std::cout << "[ " << __DATE__ << " | " << __TIME__ << " ]: " << msg
            << std::endl;
}

void fatal_error(std::string error_message) {
  perror(std::string("Fatal Error: " + error_message).c_str());
  exit(1);
}
} // namespace utility