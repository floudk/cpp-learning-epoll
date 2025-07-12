# Modern C++ I/O Simple Server

A simple server written in modern C++ that contains implementations of various I/O models: BIO, Select, Poll, and Epoll. This project is aimed at learning and comparing different I/O models in a straightforward manner and not intended for production use. Along with the server implementations, it includes a performance testing framework to evaluate the efficiency of each model.


## Features

### network I/O models
- **BIO (Blocking I/O)**: Each connection is handled by a separate thread, which is straightforward but resource-intensive.
- **Select**: The basic implementation of I/O multiplexing, limited by the maximum number of file descriptors (usually 1024).
- **Poll**: Similar to Select but without the file descriptor limit, allowing for more connections.
- **Epoll**: The most efficient I/O model for Linux, supporting edge-triggered events and high concurrency.
- **IO_URING**: A modern asynchronous I/O model introduced in Linux 5.1, which allows for high-performance I/O operations with reduced system call overhead. 


Note: The server implementations are designed to be simple and focus on the mechanisms rather than performance optimizations, so except for the BIO model, the other models do not implement multithreading optimizations. When testing the server, you may find the performance of the BIO model is the best, but this is due to the simplicity of the implementation rather than its efficiency.

### Modern C++ Features

1. **RAII (Resource Acquisition Is Initialization)**: Ensures resources are properly managed and released.
2. **Smart Pointers**: Utilizes `std::shared_ptr` and `std::unique_ptr` for memory management.
3. **Atomic Operations**: Uses `std::atomic` for thread-safe operations.
4. **Threading**: Implements multithreading using `std::thread` for concurrent request handling.
5. **Move Semantics**: Supports efficient resource transfer using move semantics.
6. **variant**: Uses `std::variant` for handling different types of data in a type-safe manner.


## Test File

The `test/client.cpp` offers a simple client implementation to test the server. 
`./benchmark-client -c 2000 -m 100 -i 5 -p 18081` can be used to run the client against the server, where: -c is the number of clients, -m is the number of messages per client, -i is the interval between messages, and -p is the port number of the server.  

After running the client, the result will show in standard output.
