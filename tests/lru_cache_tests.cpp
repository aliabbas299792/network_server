#include <chrono>
#include <fcntl.h>
#include <fstream>
#include <cstdio>
#include <iostream>
#include <sys/inotify.h>
#include <thread>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "vendor/doctest/doctest/doctest.h"

// need to access private members to access inotify stuff
#define private public
#include "subprojects/event_manager/event_manager.hpp"
#include "tests.hpp"
#include "header/lru.hpp"

std::string long_message = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Quisque cursus "
                                 "iaculis felis ut faucibus. Pellentesque sed eleifend ipsum. Aenean eget "
                                 "neque eu diam lobortis sodales. Nam gravida nisl in lacus convallis, sed "
                                 "convallis ante rutrum. Nam facilisis massa leo, quis hendrerit nisi "
                                 "accumsan et. Praesent id orci nec lorem varius elementum eu eget purus. "
                                 "Nullam laoreet suscipit leo sit amet sodales. Mauris non nibh sit amet "
                                 "est faucibus ullamcorper. Maecenas imperdiet velit ut blandit semper. "
                                 "Proin ultrices luctus nulla eleifend pharetra. Vivamus ac feugiat quam. "
                                 "Vivamus venenatis auctor neque vel lacinia. Nulla lorem ipsum, ultrices "
                                 "sed odio vel, mattis aliquet odio. Nam suscipit in lacus eget volutpat. "
                                 "Cras lorem quam, interdum sit amet sem a, congue blandit urna. Lorem "
                                 "ipsum dolor sit amet, consectetur adipiscing elit. Quisque cursus iaculis "
                                 "felis ut faucibus. Pellentesque sed eleifend ipsum. Aenean eget neque eu "
                                 "diam lobortis sodales. Nam gravida nisl in lacus convallis, sed convallis "
                                 "ante rutrum. Nam facilisis massa leo, quis hendrerit nisi accumsan et. "
                                 "Praesent id orci nec lorem varius elementum eu eget purus. Nullam laoreet "
                                 "suscipit leo sit amet sodales. Mauris non nibh sit amet est faucibus "
                                 "ullamcorper. Maecenas imperdiet velit ut blandit semper. Proin ultrices "
                                 "luctus nulla eleifend pharetra. Vivamus ac feugiat quam. Vivamus "
                                 "venenatis auctor neque vel lacinia. Nulla lorem ipsum, ultrices sed odio "
                                 "vel, mattis aliquet odio. Nam suscipit in lacus eget volutpat. Cras lorem "
                                 "quam, interdum sit amet sem a, congue blandit urna. Lorem ipsum dolor sit "
                                 "amet, consectetur adipiscing elit. Quisque cursus iaculis felis ut "
                                 "faucibus. Pellentesque sed eleifend ipsum. Aenean eget neque eu diam "
                                 "lobortis sodales. Nam gravida nisl in lacus convallis, sed convallis ante "
                                 "rutrum. Nam facilisis massa leo, quis hendrerit nisi accumsan et. "
                                 "Praesent id orci nec lorem varius elementum eu eget purus. Nullam laoreet "
                                 "suscipit leo sit amet sodales. Mauris non nibh sit amet est faucibus "
                                 "ullamcorper. Maecenas imperdiet velit ut blandit semper. Proin ultrices "
                                 "luctus nulla eleifend pharetra. Vivamus ac feugiat quam. Vivamus "
                                 "venenatis auctor neque vel lacinia. Nulla lorem ipsum, ultrices sed odio "
                                 "vel, mattis aliquet odio. Nam suscipit in lacus eget volutpat. Cras lorem "
                                 "quam, interdum sit amet sem a, congue blandit urna.";

char *create_buffer() {
  return malloc_str(long_message);
}

TEST_CASE("LRU tests") {
  SUBCASE("Try to remove orignal 2 items by inserting more") {
    lru_file_cache cache{2};
    
    char *buff1 = create_buffer();
    char *buff2 = create_buffer();
    char *buff3 = create_buffer();
    char *buff4 = create_buffer();

    REQUIRE(cache.add_or_update_item("long_message1", buff1, long_message.size()+1));
    REQUIRE(cache.add_or_update_item("long_message2", buff2, long_message.size()+1));
    REQUIRE(cache.add_or_update_item("long_message3", buff3, long_message.size()+1));
    REQUIRE(cache.add_or_update_item("long_message4", buff4, long_message.size()+1));

    REQUIRE(cache.get_and_lock_item("long_message3"));
    REQUIRE(cache.get_and_lock_item("long_message4"));
    REQUIRE_FALSE(cache.get_and_lock_item("long_message1"));
    REQUIRE_FALSE(cache.get_and_lock_item("long_message2"));
  }

  SUBCASE("No duplicates") {
    lru_file_cache cache{2};
    
    char *buff1 = create_buffer();
    char *buff2 = create_buffer();

    REQUIRE(cache.add_or_update_item("long_message1", buff1, long_message.size()+1));
    REQUIRE(cache.add_or_update_item("long_message1", buff2, long_message.size()+1));

    REQUIRE(cache.remove_item("long_message1"));
    REQUIRE(cache.get_and_lock_item("long_message1") == nullptr);
  }

  SUBCASE("Can't update item if it is locked") {
    lru_file_cache cache{2};
    
    char *buff1 = create_buffer();
    char *buff2 = create_buffer();

    REQUIRE(cache.add_or_update_item("long_message1", buff1, long_message.size()+1));
    
    REQUIRE(cache.get_and_lock_item("long_message1") != nullptr);

    REQUIRE_FALSE(cache.add_or_update_item("long_message1", buff2, long_message.size()+1));

    FREE(buff2);
  }

  SUBCASE("Can't add to cache if everything is locked") {
    lru_file_cache cache{2};
    
    char *buff1 = create_buffer();
    char *buff2 = create_buffer();
    char *buff3 = create_buffer();

    REQUIRE(cache.add_or_update_item("long_message1", buff1, long_message.size()+1));
    REQUIRE(cache.add_or_update_item("long_message2", buff2, long_message.size()+1));

    REQUIRE(cache.get_and_lock_item("long_message1") != nullptr);
    REQUIRE(cache.get_and_lock_item("long_message2") != nullptr);

    REQUIRE_FALSE(cache.add_or_update_item("long_message3", buff3, long_message.size()+1));

    FREE(buff3);
  }

  SUBCASE("Can't add to empty cache") {
    lru_file_cache cache{0};
    
    char *buff1 = create_buffer();

    REQUIRE_FALSE(cache.add_or_update_item("long_message1", buff1, long_message.size()+1));

    FREE(buff1);
  }

  SUBCASE("Get and lock middle item of 3 items") {
    lru_file_cache cache{3};
    
    char *buff1 = create_buffer();
    char *buff2 = create_buffer();
    char *buff3 = create_buffer();

    REQUIRE(cache.add_or_update_item("long_message1", buff1, long_message.size()+1));
    REQUIRE(cache.add_or_update_item("long_message2", buff2, long_message.size()+1));
    REQUIRE(cache.add_or_update_item("long_message3", buff3, long_message.size()+1));

    REQUIRE(cache.get_and_lock_item("long_message2"));
  }

  SUBCASE("Remove the middle item of 3 items") {
    lru_file_cache cache{3};
    
    char *buff1 = create_buffer();
    char *buff2 = create_buffer();
    char *buff3 = create_buffer();

    REQUIRE(cache.add_or_update_item("long_message1", buff1, long_message.size()+1));
    REQUIRE(cache.add_or_update_item("long_message2", buff2, long_message.size()+1));
    REQUIRE(cache.add_or_update_item("long_message3", buff3, long_message.size()+1));

    REQUIRE(cache.remove_item("long_message2"));
  }

  SUBCASE("Unlock an item") {
    lru_file_cache cache{3};
    
    char *buff1 = create_buffer();
    char *buff2 = create_buffer();
    char *buff3 = create_buffer();

    REQUIRE(cache.add_or_update_item("long_message1", buff1, long_message.size()+1));
    REQUIRE(cache.add_or_update_item("long_message2", buff2, long_message.size()+1));
    REQUIRE(cache.add_or_update_item("long_message3", buff3, long_message.size()+1));

    REQUIRE(cache.get_and_lock_item("long_message2"));
    cache.unlock_item("long_message2");
  }

  SUBCASE("Try to remove an item that doesn't exist") {
    lru_file_cache cache{3};
    
    REQUIRE(cache.remove_item("long_message1"));
  }

  SUBCASE("Item updated due to inotify event deletes the item") {
    event_manager ev{};
    application_methods app{};
    network_server ns{4001, &ev, &app};

    auto t1 = std::thread([&] {
      ns.start();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string filename = "inotify_test_event_file.txt";

    char *b1 = create_buffer();
    char *b2 = create_buffer();
    char *b3 = create_buffer();
    REQUIRE(ns.cache.add_or_update_item("long_msg1", b1, long_message.length()+1));
    REQUIRE(ns.cache.add_or_update_item("long_msg2", b2, long_message.length()+1));
    REQUIRE(ns.cache.add_or_update_item("long_msg3", b3, long_message.length()+1));

    std::ofstream test_file(filename);
    test_file << long_message;
    test_file.close();

    // add the file - passes buff1 to cache
    char *buff1 = create_buffer(); // buff1 and long_message have the same data so this is fine
    REQUIRE(ns.cache.add_or_update_item(filename, buff1, long_message.length()+1));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // update the file - should remove it from the cache
    test_file = std::ofstream(filename, std::ios_base::app);
    test_file << long_message;
    test_file.close();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // check if it's actually gone from the cache
    REQUIRE(ns.cache.get_and_lock_item(filename) == nullptr);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // // add the file - pass buff1 to the cache again
    buff1 = create_buffer();
    REQUIRE(ns.cache.add_or_update_item(filename, buff1, long_message.length()+1));

    // // lock the file
    REQUIRE(ns.cache.get_and_lock_item(filename) != nullptr);

    // update the file - should remove it from the cache still even though it's locked right now
    test_file = std::ofstream(filename, std::ios_base::app);
    test_file << long_message;
    test_file.close();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // check if it's actually gone from the cache
    REQUIRE(ns.cache.get_and_lock_item(filename) == nullptr);

    ns.stop();
    t1.join();
  }

  SUBCASE("Process event doesn't try to process if there's no event") {
    // so there should be no change to cache items
    lru_file_cache cache{3};

    char *b1 = create_buffer();
    char *b2 = create_buffer();
    char *b3 = create_buffer();
    REQUIRE(cache.add_or_update_item("long_msg1", b1, long_message.length()+1));
    REQUIRE(cache.add_or_update_item("long_msg2", b2, long_message.length()+1));
    REQUIRE(cache.add_or_update_item("long_msg3", b3, long_message.length()+1));

    cache.process_inotify_event(nullptr);

    REQUIRE(cache.get_and_lock_item("long_msg1"));
    REQUIRE(cache.get_and_lock_item("long_msg2"));
    REQUIRE(cache.get_and_lock_item("long_msg3"));

    inotify_event e;
    e.mask = 0;
    cache.process_inotify_event(&e);

    REQUIRE(cache.get_and_lock_item("long_msg1"));
    REQUIRE(cache.get_and_lock_item("long_msg2"));
    REQUIRE(cache.get_and_lock_item("long_msg3"));
  }
}
