#include "server.hpp"

void IOUringServer::run(uint16_t port){

    if (io_uring_queue_init(2048, &ring_, IORING_SETUP_SQPOLL) < 0) {
        Logger::error("Failed to initialize io_uring");
        return;
    }

    auto server_fd_opt = init_socket(port, get_name());
    if (!server_fd_opt.has_value()) {
        Logger::error("Failed to create socket");
        return;
    }
    server_fd_ = std::move(server_fd_opt.value());

    set_non_blocking(server_fd_.get());

    // add server socket to io_uring
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    if (!sqe) {
        Logger::error("Failed to get sqe");
        return;
    }
    
    io_uring_prep_accept(sqe, server_fd_.get(), nullptr, nullptr, 0);
    sqe->user_data = 0x100000000;

    cqes_.resize(2048);

    while(running_){
        int submitted = io_uring_submit(&ring_);
        if (submitted < 0) {
            Logger::error("Failed to submit io_uring requests: ", strerror(-submitted));
            break;
        }
        
        // If no requests were submitted, we might be waiting for completions
        if (submitted == 0) {
            // Small delay to prevent busy waiting
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }

        int ret = io_uring_wait_cqe(&ring_, &cqes_[0]);
        if (ret < 0) {
            Logger::error("Failed to wait for io_uring completions: ", strerror(-ret));
            continue;
        }

        int cqe_count = io_uring_peek_batch_cqe(&ring_, cqes_.data(), cqes_.size());
        if (cqe_count < 0){
            cqe_count = 1;
        }

        for (int i = 0; i < cqe_count; ++i){
            struct io_uring_cqe* cqe = cqes_[i];
            if (cqe->res < 0) {
                uint64_t user_data = cqe->user_data;
                int fd = static_cast<int>(user_data & 0xFFFFFFFF);
                if (user_data & 0x100000000) {
                    // accept error, try again
                    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
                    if(sqe){
                        io_uring_prep_accept(sqe, server_fd_.get(), nullptr, nullptr, 0);
                        sqe->user_data = 0x100000000;
                    } else {
                        // Try to submit pending requests and retry
                        io_uring_submit(&ring_);
                        sqe = io_uring_get_sqe(&ring_);
                        if(sqe){
                            io_uring_prep_accept(sqe, server_fd_.get(), nullptr, nullptr, 0);
                            sqe->user_data = 0x100000000;
                        } else {
                            Logger::error("Failed to get sqe for accept retry after retry");
                        }
                    }
                }else{
                    // client read error, close connection
                    auto it = clients_.find(fd);
                    if (it != clients_.end()){
                        cleanup_client(it->second.get());
                    }
                }
            }else{
                uint64_t user_data = cqe->user_data;
                int fd = static_cast<int>(user_data & 0xFFFFFFFF);
                if (user_data & 0x100000000){
                    // accept success
                    int client_fd = cqe->res;
                    set_non_blocking(client_fd);

                    auto ctx = std::make_unique<ClientContext>(client_fd);
                    clients_[client_fd] = std::move(ctx);
                    active_connections_++;

                    handle_client_read(clients_[client_fd].get());

                    // submit next accept request
                    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
                    if(sqe){
                        io_uring_prep_accept(sqe, server_fd_.get(), nullptr, nullptr, 0);
                        sqe->user_data = 0x100000000;
                    } else {
                        // Try to submit pending requests and retry
                        io_uring_submit(&ring_);
                        sqe = io_uring_get_sqe(&ring_);
                        if(sqe){
                            io_uring_prep_accept(sqe, server_fd_.get(), nullptr, nullptr, 0);
                            sqe->user_data = 0x100000000;
                        } else {
                            Logger::error("Failed to get sqe for next accept after retry");
                        }
                    }
                }else{
                    // client read success
                    auto it = clients_.find(fd);
                    if (it != clients_.end()){
                        ClientContext* ctx = it->second.get();
                        if (ctx->is_reading){
                            ctx->is_reading = false;
                            ctx->buffer_size = cqe->res;

                            if (ctx->buffer_size > 0){
                                total_messages_++;
                                handle_client_write(ctx);
                            }else{
                                // client closed connection
                                cleanup_client(ctx);
                            }
                        }else if(ctx->is_writing){
                            ctx->is_writing = false;
                            handle_client_read(ctx);
                        }
                    }
                }
            }
            
            // Mark this completion as seen
            io_uring_cqe_seen(&ring_, cqe);
        }
    }

    io_uring_queue_exit(&ring_);
    Logger::info("Server stopped");
    if (stats_thread_.joinable()) {
        stats_thread_.join();
    }
}


void IOUringServer::cleanup_client(ClientContext* ctx){
    close(ctx->client_fd);
    clients_.erase(ctx->client_fd);
    active_connections_--;
}

void IOUringServer::handle_client_read(ClientContext* ctx){
    if (ctx->is_reading) return; // already reading

    ctx->is_reading = true;
    ctx->buffer_size = 0;

    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    if (!sqe) {
        // Try to submit pending requests and retry
        io_uring_submit(&ring_);
        sqe = io_uring_get_sqe(&ring_);
        if (!sqe) {
            Logger::error("Failed to get sqe for read after retry, closing client");
            cleanup_client(ctx);
            return;
        }
    }

    io_uring_prep_recv(sqe, ctx->client_fd, ctx->buffer, sizeof(ctx->buffer), 0);
    sqe->user_data = ctx->client_fd;
}

void IOUringServer::handle_client_write(ClientContext* ctx){
    if (ctx->is_writing) return; // already writing

    ctx->is_writing = true;

    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    if (!sqe) {
        // Try to submit pending requests and retry
        io_uring_submit(&ring_);
        sqe = io_uring_get_sqe(&ring_);
        if (!sqe) {
            Logger::error("Failed to get sqe for write after retry, closing client");
            cleanup_client(ctx);
            return;
        }
    }

    std::string response = "Echo: " + std::string(ctx->buffer, ctx->buffer_size);
    if (ctx->buffer_size > 0 && ctx->buffer[ctx->buffer_size - 1] == '\n') {
        response.pop_back(); 
    }
    response += " (Msg #" + std::to_string(total_messages_.load() + 1) + ")\n";
    
    size_t response_size = std::min(response.size(), sizeof(ctx->buffer));
    if (response_size > 0) {
        std::memcpy(ctx->buffer, response.c_str(), response_size);
    }

    io_uring_prep_send(sqe, ctx->client_fd, ctx->buffer, response_size, 0);
    sqe->user_data = ctx->client_fd;
}

