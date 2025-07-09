cmake_minimum_required(VERSION 3.10)
project(cpp-io-learning)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

# Source files
set(SOURCES
    src/bio_server.cpp
    src/utils.cpp
    src/main.cpp
    src/select_server.cpp
)

add_executable(cpp-io-learning ${SOURCES})
