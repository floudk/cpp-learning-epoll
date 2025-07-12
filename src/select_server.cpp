#include "server.hpp"


void SelectServer::run(uint16_t port){
    auto server_fd_opt = init_socket(port, get_name());
    if (!server_fd_opt.has_value()) {
        Logger::error("Failed to create socket");
        return;
    }
    auto server_fd = std::move(server_fd_opt.value());
    // init fd_set,
    // which essentially is a bitmask of file descriptors
    fd_set read_fds, master_fds;
    FD_ZERO(&master_fds);
    // add server_fd to master_fds, that is, we will monitor server_fd for new connections
    FD_SET(server_fd.get(), &master_fds);

    int max_fd = server_fd.get();
    std::vector<SocketRAII> client_fds;

    while(running_) {
        read_fds = master_fds; // copy master_fds to read_fds
        int nready = select(max_fd + 1, &read_fds, nullptr, nullptr, nullptr);
        if (nready == -1) {
            if (running_) {
                Logger::error("Failed to select");
            }
            continue;
        }

        // check if there is a new connection
        if (FD_ISSET(server_fd.get(), &read_fds)) {
            sockaddr_in client_addr{};
            socklen_t client_addr_len = sizeof(client_addr);
            int client_fd = ::accept(server_fd.get(), reinterpret_cast<sockaddr*>(&client_addr), &client_addr_len);
            if (client_fd == -1) {
                if (running_) {
                    Logger::error("Failed to accept connection");
                }
                continue;
            }
            client_fds.emplace_back(client_fd);
            FD_SET(client_fd, &master_fds);
            max_fd = std::max(max_fd, client_fd);
            active_connections_++;

            // Logger::info("New connection from ", client_fd);
        }

        // check if there is data to read from any client
        for (auto it = client_fds.begin(); it != client_fds.end();) {
            int client_fd = it->get();
            if (FD_ISSET(client_fd, &read_fds)) {
                if (!handle_client_data(client_fd)) {
                    // 连接关闭
                    FD_CLR(client_fd, &master_fds);
                    it = client_fds.erase(it);
                    active_connections_--;
                    
                    // Logger::info("Client-", client_fd, " disconnected. Active connections: ", active_connections_.load());
                    continue;
                }
            }
            ++it;
        }
        
    }
    client_fds.clear();
    Logger::info("Server stopped");
    if (stats_thread_.joinable()) {
        stats_thread_.join();
    }
}


bool SelectServer::handle_client_data(int client_fd){
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