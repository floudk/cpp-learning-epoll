#include "server.hpp"



void BioServer::run(uint16_t port) {
    auto server_fd_opt = init_socket(port, get_name());
    if (!server_fd_opt.has_value()) {
        Logger::error("Failed to create socket");
        return;
    }
    auto server_fd = std::move(server_fd_opt.value());

   
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

    if (stats_thread_.joinable()) {
        stats_thread_.join();
    }
    Logger::info(get_name(), " stopped");
}


void BioServer::handle_client(int client_fd) {
    SocketRAII client_socket(client_fd);
    active_connections_++;

    char buffer[1024];
    std::string clinet_info = "Client-" + std::to_string(client_fd);
    // Logger::info(clinet_info, " connected(", active_connections_.load(std::memory_order_relaxed), ")");

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

