#pragma once
#include "utils.hpp"

#include <map>


struct ServerStats {
    std::atomic<int> active_connections_{0};
    std::atomic<long long> total_messages_{0};
    std::atomic<bool> running_{false};
    std::thread stats_thread_;

    std::atomic<int>& get_active_connections() {
        return active_connections_;
    }

    std::atomic<long long>& get_total_messages() {
        return total_messages_;
    }

    std::optional<SocketRAII> init_socket(uint16_t port, std::string server_name){
        SocketRAII server_fd(socket(AF_INET, SOCK_STREAM, 0));
        if (server_fd.get() == -1) {
                Logger::error("Failed to create socket");
                return std::nullopt;
        }
        if (!set_reuseaddr(server_fd.get())) {
            Logger::error("Failed to set reuseaddr");
            return std::nullopt;
        }
        
        sockaddr_in addr{};
        addr.sin_family = AF_INET; // IPv4
        addr.sin_port = htons(port); // host to network
        addr.sin_addr.s_addr = INADDR_ANY; // listen on all interfaces
        
        if(::bind(server_fd.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
            Logger::error("Failed to bind socket");
            return std::nullopt;
        }

        if(::listen(server_fd.get(), SOMAXCONN) == -1) {
            Logger::error("Failed to listen");
            return std::nullopt;
        }

        Logger::info(server_name, " started on port ", port);
        running_ = true;

        // statstics
        stats_thread_ = std::thread([this, server_name]() {
            while(running_) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                print_stats(server_name, active_connections_, total_messages_);
            }
        });
        return server_fd;
    }

    void stop(){
        running_ = false;
    }
};

class BioServer: public ServerStats{
public:
    std::string get_name() const {
        return "BioServer";
    }

    void run(uint16_t port);
private:
    void handle_client(int client_fd);
};

class SelectServer: public ServerStats{
public:
    std::string get_name() const {
        return "SelectServer";
    }

    void run(uint16_t port);
private:
    bool handle_client_data(int client_fd);
};

class PollServer: public ServerStats{
public:
    std::string get_name() const {
        return "PollServer";
    }

    void run(uint16_t port);
private:
    bool handle_client_data(int client_fd);
};



class EpollServer: public ServerStats{
public:
    std::string get_name() const {
        return "EpollServer";
    }

    void run(uint16_t port);

private:
    bool handle_client_data(int client_fd);
    void handle_new_connection(int epoll_fd, int server_fd);
};


class IOUringServer: public ServerStats{
public:
    std::string get_name() const {
        return "IOUringServer";
    }

    void run(uint16_t port);
private:
    struct ClientContext{
        int client_fd;
        char buffer[1024];
        size_t buffer_size;
        bool is_writing;
        bool is_reading;

        ClientContext(int fd) : client_fd(fd), is_writing(false), is_reading(false) {}
    };
    
    SocketRAII server_fd_;  
    struct io_uring ring_;
    std::map<int, std::unique_ptr<ClientContext>> clients_;
    std::vector<struct io_uring_cqe*> cqes_;
    void setup_server_socket(uint16_t port);
    void handle_client_read(ClientContext* ctx);
    void handle_client_write(ClientContext* ctx);
    void cleanup_client(ClientContext* ctx);
    void process_completions();
    
    
};


enum class ServerKind { Bio, Select, Poll, Epoll, IOUring };
class Server{
    using V = std::variant<BioServer, SelectServer, PollServer, EpollServer, IOUringServer>;
    V impl_;
public:
    template<typename T, typename... Args>
    explicit Server(std::in_place_type_t<T>, Args&&... args)
    : impl_(std::in_place_type<T>, std::forward<Args>(args)...) 
    {}

    static Server make(ServerKind kind){
        switch(kind){
            case ServerKind::Bio:
                return Server{std::in_place_type<BioServer>};
            case ServerKind::Select:
                return Server{std::in_place_type<SelectServer>};
            case ServerKind::Poll:
                return Server{std::in_place_type<PollServer>};
            case ServerKind::Epoll:
                return Server{std::in_place_type<EpollServer>};
            case ServerKind::IOUring:
                return Server{std::in_place_type<IOUringServer>};
        }
        std::terminate();
    }

    void run(uint16_t port){
        std::visit([port](auto& server){
            server.run(port);
        }, impl_);
    }

    void stop(){
        std::visit([](auto& server){
            server.stop();
        }, impl_);
    }

    std::string get_name() const {
        return std::visit([](auto& server){
            return server.get_name();
        }, impl_);
    }

    std::atomic<int>& get_active_connections() {
        return std::visit([](auto& server)-> std::atomic<int>&{
            return server.get_active_connections();
        }, impl_);
    }

    std::atomic<long long>& get_total_messages() {
        return std::visit([](auto& server)-> std::atomic<long long>&{
            return server.get_total_messages();
        }, impl_);
    }
};