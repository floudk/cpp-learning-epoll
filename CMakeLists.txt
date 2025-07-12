cmake_minimum_required(VERSION 3.10)
project(cpp-io-learning)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Threads REQUIRED)

include_directories(include)

# Source files
set(SOURCES
    src/bio_server.cpp
    src/utils.cpp
    src/main.cpp
    src/select_server.cpp
    src/poll_server.cpp
    src/epoll_server.cpp
    src/io_uring.cpp
)

add_executable(cpp-io-learning ${SOURCES})

# Link io_uring library
target_link_libraries(cpp-io-learning uring)
target_link_libraries(cpp-io-learning Threads::Threads)

add_executable(benchmark-client test/client.cpp)
target_link_libraries(benchmark-client Threads::Threads)


if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(cpp-io-learning PRIVATE -Wall -Wextra -O2)
    target_compile_options(benchmark-client PRIVATE -Wall -Wextra -O2)
endif() 