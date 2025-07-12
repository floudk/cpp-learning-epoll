#include "server.hpp"





void EpollServer::run(uint16_t port){
    auto server_fd_opt = init_socket(port, get_name());
    if (!server_fd_opt.has_value()) {
        Logger::error("Failed to create socket");
        return;
    }
    auto server_fd = std::move(server_fd_opt.value());

    int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd == -1) {
        Logger::error("Failed to create epoll instance");
        return;
    }

    epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = server_fd.get();

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd.get(), &event) == -1) {
        Logger::error("Failed to add server socket to epoll");
        close(epoll_fd);
        return;
    }
    std::vector<epoll_event> events(1024);
    while(running_){
        int nready = epoll_wait(epoll_fd, events.data(), events.size(), -1);
        if (nready == -1) {
            if (running_) {
                Logger::error("Failed to wait for events");
            }
        }
        for (int i = 0; i < nready; ++i) {
            int fd = events[i].data.fd;

            if (fd == server_fd.get()) {
                handle_new_connection(epoll_fd, server_fd.get());
            } else {
                if (!handle_client_data(fd)) {
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
                    close(fd);
                    active_connections_--;
                }
            }
        }
    }
    close(epoll_fd);
    Logger::info("Server stopped");
    if (stats_thread_.joinable()) {
        stats_thread_.join();
    }
}

void EpollServer::handle_new_connection(int epoll_fd, int server_fd){
    sockaddr_in client_addr{};
    socklen_t client_addr_len = sizeof(client_addr);

    int client_fd = ::accept(server_fd, reinterpret_cast<sockaddr*>(&client_addr), &client_addr_len);
    if (client_fd == -1) {
        Logger::error("Failed to accept new connection");
        return;
    }

    set_non_blocking(client_fd);

    epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = client_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
        Logger::error("Failed to add client socket to epoll");
        close(client_fd);
        return;
    }
    active_connections_++;
}


bool EpollServer::handle_client_data(int client_fd){
    char buffer[1024];
    ssize_t total_read = 0;

    while(true){
        ssize_t bytes_read = recv(client_fd, buffer + total_read, 
            sizeof(buffer) - total_read - 1, 0);
        if (bytes_read == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            return false;
        }
        if (bytes_read == 0) {
            return true;
        }
        total_read += bytes_read;
        if (total_read > 0 && buffer[total_read - 1] == '\n') {
            break;
        }
    }

    if (total_read > 0) {
        buffer[total_read] = '\0';
        std::string response = "Echo[" + std::to_string(total_messages_.load(std::memory_order_relaxed)) + "]:" + std::string(buffer);
        if (buffer[total_read - 1] == '\n') {
            response.pop_back();
        }
        if (::send(client_fd, response.c_str(), response.size(), 0) == -1) {
            Logger::error("Failed to send response to client");
            return false;
        }
        total_messages_++;
    }
    return true;
}