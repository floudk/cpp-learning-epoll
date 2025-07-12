#include "server.hpp"


void PollServer::run(uint16_t port){
    auto server_fd_opt = init_socket(port, get_name());
    if (!server_fd_opt.has_value()) {
        Logger::error("Failed to create socket");
        return;
    }
    auto server_fd = std::move(server_fd_opt.value());

    // init poll fd
    std::vector<pollfd> poll_fds;
    poll_fds.emplace_back(server_fd.get(), POLLIN);

    while(running_){
        int nready = poll(poll_fds.data(), poll_fds.size(), -1); // copy poll_fds to kernel space (every time)
        if (nready == -1) {
            if (running_) {
                Logger::error("Failed to poll");
            }
            continue;
        }
        if (poll_fds[0].revents & POLLIN) {
            sockaddr_in client_addr{};
            socklen_t client_addr_len = sizeof(client_addr);
            int client_fd = ::accept(server_fd.get(), reinterpret_cast<sockaddr*>(&client_addr), &client_addr_len);
            if (client_fd == -1) {
                if (running_) {
                    Logger::error("Failed to accept connection");
                }
                continue;
            }

            poll_fds.emplace_back(client_fd, POLLIN);
            active_connections_++;
            // Logger::info("New connection from ", client_fd);
        }
        for (auto it = poll_fds.begin() + 1; it != poll_fds.end();) { // 跳过监听fd
            int client_fd = it->fd;
            
            // 优先处理断开和错误事件
            if (it->revents & (POLLHUP | POLLERR)) {
                ::close(client_fd);
                it = poll_fds.erase(it);
                active_connections_--;
                // Logger::info("Client-", client_fd, " disconnected (HUP/ERR). Active connections: ", active_connections_.load());
                continue;
            }
            
            // 处理可读事件
            if (it->revents & POLLIN) {
                if (!handle_client_data(client_fd)) {
                    ::close(client_fd);
                    it = poll_fds.erase(it);
                    active_connections_--;
                    // Logger::info("Client-", client_fd, " disconnected (recv failed). Active connections: ", active_connections_.load());
                    continue;
                }
            }
            ++it;
        }

        
    }
    for (auto& pfd : poll_fds) {
        ::close(pfd.fd);
    }
    poll_fds.clear();
    Logger::info("Server stopped");
    if (stats_thread_.joinable()) {
        stats_thread_.join();
    }
}


bool PollServer::handle_client_data(int client_fd){
    char buffer[1024];
    ssize_t bytes_read = ::recv(client_fd, buffer, sizeof(buffer)-1, 0);
    if (bytes_read <= 0) {
        return false;
    }
    buffer[bytes_read] = '\0';
    std::string response = "Echo[" + std::to_string(total_messages_.load(std::memory_order_relaxed)) + "]:" + std::string(buffer);
    if (buffer[bytes_read] == '\n'){
        response.pop_back(); 
    }
    if (::send(client_fd, response.c_str(), response.size(), 0) == -1) {
        Logger::error("Failed to send response to client");
        return false;
    }
    total_messages_++;
    return true;
}