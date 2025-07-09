#include "server.hpp"
#include "utils.hpp"

struct BioServer::SocketRAII {
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


void BioServer::run(uint16_t port) {
   SocketRAII server_fd(socket(AF_INET, SOCK_STREAM, 0));
   if (server_fd.get() == -1) {
        Logger::error("Failed to create socket");
        return;
   }
   if (!set_reuseaddr(server_fd.get())) {
    Logger::error("Failed to set reuseaddr");
    return;
   }
   
   sockaddr_in addr{};
   addr.sin_family = AF_INET; // IPv4
   addr.sin_port = htons(port); // host to network
   addr.sin_addr.s_addr = INADDR_ANY; // listen on all interfaces
   
   if(::bind(server_fd.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
    Logger::error("Failed to bind socket");
    return;
   }

   if(::listen(server_fd.get(), SOMAXCONN) == -1) {
    Logger::error("Failed to listen");
    return;
   }

    Logger::info(get_name(), " started on port ", port);
    running_ = true;

    // statstics
    std::thread stats_thread([this]() {
        while(running_) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            print_stats(get_name(), active_connections_, total_messages_);
        }
    });
   
    while(running_) {
        sockaddr_in client_addr{};
        socklen_t client_addr_len = sizeof(client_addr);
        int client_fd = ::accept(server_fd.get(), reinterpret_cast<sockaddr*>(&client_addr), &client_addr_len);
        if (client_fd == -1) {
            if (running_){
                Logger::error("Failed to accept connection");
            }
            continue;
        }

        // handle connection in new thread
        std::thread client_thread([this, client_fd]() {
            handle_client(client_fd);
        });
        client_thread.detach();
    }
    if (stats_thread.joinable()) {
        stats_thread.join();
    }
    Logger::info(get_name(), " stopped");
}

void BioServer::stop() {
    running_ = false;
}

void BioServer::handle_client(int client_fd) {
    SocketRAII client_socket(client_fd);
    active_connections_++;

    char buffer[1024];
    std::string clinet_info = "Client-" + std::to_string(client_fd);
    Logger::info(clinet_info, " connected(", active_connections_.load(std::memory_order_relaxed), ")");

    while(running_) {
        ssize_t bytes_read = ::recv(client_socket.get(), buffer, sizeof(buffer)-1, 0);
        if (bytes_read <= 0) {
            break;
        }
        buffer[bytes_read] = '\0';
        std::string response = "Echo[" + std::to_string(total_messages_.load(std::memory_order_relaxed)) + "]:" + std::string(buffer);
        if (buffer[bytes_read] == '\n'){
            response.pop_back(); 
        }
        if (::send(client_socket.get(), response.c_str(), response.size(), 0) == -1) {
            Logger::error(clinet_info, " failed to send response");
            break;
        }
        total_messages_++;
    }

}

