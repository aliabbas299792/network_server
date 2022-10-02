#include <array>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <ranges>
#include <set>
#include <string>
#include <unistd.h>
#include <vector>

struct post_content {
  const char *name{};
  const char *filename{};
  const char *content_type{};
  const char *content_disposition{};
  size_t size{};
  uint8_t *buff{};
};

struct test {
  std::string boundary_str{};
  std::string boundary_last_str{};
  const char CRLF[4] = "\r\n";
  const char double_CRLF[8] = "\r\n\r\n";
  size_t break_len = strlen(CRLF);
  size_t double_break_len = strlen(double_CRLF);
  char *content = (char *)calloc(10000000, 1);
  size_t content_length{};
  std::vector<post_content> items{};

  ~test() { free(content); }

  const std::vector<post_content> &extract_data_items() {
    if (boundary_str == "" || boundary_last_str == "")
      return items;

    auto boundary = boundary_str.c_str();
    auto boundary_last = boundary_last_str.c_str();

    void *item_ptr = content;
    char *max_ptr = (char *)item_ptr + content_length - 1;
    size_t remaining_len = content_length;
    while ((item_ptr = memmem(item_ptr, remaining_len, boundary, boundary_str.size())) != nullptr) {
      item_ptr = (char *)item_ptr + boundary_str.size(); // increment past the boundary
      char *data_ptr = (char *)item_ptr;

      if (data_ptr < max_ptr) {
        const char content_disposition_key[] = "Content-Disposition: ";
        const char name_key[] = "name=\"";
        const char filename_key[] = "filename=\"";
        const char content_type_key[] = "Content-Type: ";

        post_content data{};

        char *next = reinterpret_cast<char *>(memmem(data_ptr, remaining_len, boundary, boundary_str.size()));
        if (!next) {
          next = reinterpret_cast<char *>(
              memmem(data_ptr, remaining_len, boundary_last, boundary_last_str.size()));
          if (!next)
            next = max_ptr;
        }
        *(next - break_len) = '\0'; // inserts null character before next section

        size_t current_section_length = (next - break_len) - data_ptr - 1; // -1 for the null character

        void *substr_ptr{};

        if ((substr_ptr = memmem(item_ptr, remaining_len, double_CRLF, double_break_len)) != nullptr) {
          char *substr_data_ptr = (char *)substr_ptr;
          substr_data_ptr += double_break_len;
          data.buff = reinterpret_cast<uint8_t *>(substr_data_ptr);
          data.size = next - substr_data_ptr - break_len;
        }

        if ((substr_ptr = memmem(data_ptr, current_section_length, content_disposition_key,
                                 strlen(content_disposition_key))) != nullptr) {
          char *substr_data_ptr = (char *)substr_ptr;
          substr_data_ptr += strlen(content_disposition_key);
          data.content_disposition = substr_data_ptr;
          if ((substr_data_ptr = strchr(substr_data_ptr, ';')) != nullptr) {
            *substr_data_ptr = '\0';
          }
        }

        if ((substr_ptr = memmem(data_ptr, current_section_length, name_key, strlen(name_key))) != nullptr) {
          char *substr_data_ptr = (char *)substr_ptr;
          substr_data_ptr += strlen(name_key);
          data.name = substr_data_ptr;
          if ((substr_data_ptr = strchr(substr_data_ptr, '"')) != nullptr) {
            *substr_data_ptr = '\0';
          }
        }

        if ((substr_ptr = memmem(data_ptr, current_section_length, filename_key, strlen(filename_key))) !=
            nullptr) {
          char *substr_data_ptr = (char *)substr_ptr;
          substr_data_ptr += strlen(filename_key);
          data.filename = substr_data_ptr;
          if ((substr_data_ptr = strchr(substr_data_ptr, '"')) != nullptr) {
            *substr_data_ptr = '\0';
          }
        }

        if ((substr_ptr = memmem(data_ptr, current_section_length, content_type_key,
                                 strlen(content_type_key))) != nullptr) {
          char *substr_data_ptr = (char *)substr_ptr;
          substr_data_ptr += strlen(content_type_key);
          data.content_type = substr_data_ptr;
          if ((substr_data_ptr = strchr(substr_data_ptr, '\n')) != nullptr) {
            *substr_data_ptr = '\0';
          }
        }

        items.push_back(data);
        remaining_len = content_length - ((char *)item_ptr - content);
      }
    }

    return items;
  }

  void make_boundaries() {
    const char boundary_tok[] = "boundary=";
    char *boundary_ptr = strstr((char *)this->content, boundary_tok);
    char *next_new_line = strstr(boundary_ptr, CRLF);
    if (boundary_ptr && next_new_line) {
      boundary_ptr += strlen(boundary_tok);
      boundary_str = "--";
      boundary_str += std::string(boundary_ptr, next_new_line - boundary_ptr);
      boundary_last_str = boundary_str + "--";
      boundary_str += CRLF;
    }
  }
};

int main() {
  test t{};

  auto fd = open("./header_example.txt", O_RDONLY);
  t.content_length = read(fd, (void *)t.content, 10000000);
  close(fd);

  // std::cout << t.content_length << " is the size\n\n";

  t.make_boundaries();

  auto items = t.extract_data_items();

  for (auto &item : items) {
    if (item.name)
      std::cout << "name: " << item.name << "\n";
    if (item.filename)
      std::cout << "filename: " << item.filename << "\n";
    if (item.content_disposition)
      std::cout << "disposition: " << item.content_disposition << "\n";
    if (item.content_type)
      std::cout << "type: " << item.content_type << "\n";
    std::cout << "size: " << item.size << "\n\n";

    auto fd = open(item.filename, O_CREAT | O_WRONLY, 0666);
    write(fd, item.buff, item.size);
    close(fd);
  }
}