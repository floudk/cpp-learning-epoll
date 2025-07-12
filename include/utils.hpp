#pragma once
#include "common.hpp"

class Logger{
public:
    template <typename ... Args>
    static void info(Args&& ... args) {
        std::lock_guard<std::mutex> lock(mtx_);
        std::cout << "[info]:";
        (std::cout << ... << std::forward<Args>(args)) << "\n";
    }


    template <typename ... Args>
    static void error(Args&& ... args) {
        std::lock_guard<std::mutex> lock(mtx_);
        std::cerr << "[error]:";
        (std::cerr << ... << std::forward<Args>(args)) << std::endl;
    }

private:
    inline static std::mutex mtx_;
};


class Timer{

private:
    std::chrono::steady_clock::time_point start_;

public:
    Timer(): start_(std::chrono::steady_clock::now()){}
    void reset() {
        start_ = std::chrono::steady_clock::now();
    }

    template <typename T = std::chrono::milliseconds>
    auto elapsed() const {
        return std::chrono::duration_cast<T>(std::chrono::steady_clock::now() - start_);
    }
};

struct SocketRAII{
    explicit SocketRAII(int fd = -1) : fd_(fd) {}
    ~SocketRAII() {
        close_fd(fd_);
    }

    SocketRAII(const SocketRAII&) = delete;
    SocketRAII& operator=(const SocketRAII&) = delete;
    SocketRAII(SocketRAII&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }
    SocketRAII& operator=(SocketRAII&& other) noexcept {
        if (this != &other) {
            close_fd(fd_);
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }
    int get() const { return fd_; }
private:
    int fd_;
    void close_fd(int fd) {
        if (fd != -1) {
            ::close(fd);
        }
    }
};


bool set_reuseaddr(int fd);
bool set_non_blocking(int fd);
std::string get_current_time();
void print_stats(std::string_view server_name, int active_connections, long long total_messages);