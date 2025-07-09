# Modern C++ I/O Simple Server

A simple server written in modern C++ that contains implementations of various I/O models: BIO, Select, Poll, and Epoll. This project is aimed at learning and comparing different I/O models in a straightforward manner and not intended for production use. Along with the server implementations, it includes a performance testing framework to evaluate the efficiency of each model.


## Features

### network I/O models
- **BIO (Blocking I/O)**: Each connection is handled by a separate thread, which is straightforward but resource-intensive.
- **Select**: The basic implementation of I/O multiplexing, limited by the maximum number of file descriptors (usually 1024).
- **Poll**: Similar to Select but without the file descriptor limit, allowing for more connections.
- **Epoll**: The most efficient I/O model for Linux, supporting edge-triggered events and high concurrency.

(Linux 5.1 introduced `io_uring`, which is not included in this project but can be considered for future enhancements.)

### Modern C++ Features

1. **RAII (Resource Acquisition Is Initialization)**: Ensures resources are properly managed and released.
2. **Smart Pointers**: Utilizes `std::shared_ptr` and `std::unique_ptr` for memory management.
3. **Atomic Operations**: Uses `std::atomic` for thread-safe operations.
4. **Threading**: Implements multithreading using `std::thread` for concurrent request handling.
5. **Move Semantics**: Supports efficient resource transfer using move semantics.
6. **CRTP** (Curiously Recurring Template Pattern): Used for implementing common interfaces across different I/O models.

## Test Framework