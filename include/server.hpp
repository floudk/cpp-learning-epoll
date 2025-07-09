#pragma once
#include "common.hpp"


struct ServerStats {
    std::atomic<int> active_connections_{0};
    std::atomic<long long> total_messages_{0};
    std::atomic<bool> running_{false};

    std::atomic<int>& get_active_connections() {
        return active_connections_;
    }

    std::atomic<long long>& get_total_messages() {
        return total_messages_;
    }

};

class BioServer: public ServerStats{
public:
    std::string get_name() const {
        return "BioServer";
    }

    void run(uint16_t port);
    void stop();
private:
    struct SocketRAII;
    void handle_client(int client_fd);
};

class SelectServer: public ServerStats{
public:
    std::string get_name() const {
        return "SelectServer";
    }

    void run(uint16_t port);
    void stop();
};

class PollServer: public ServerStats{
public:
    std::string get_name() const {
        return "PollServer";
    }

    void run(uint16_t port);
    void stop();
};



class EpollServer: public ServerStats{
public:
    std::string get_name() const {
        return "EpollServer";
    }

    void run(uint16_t port);
    void stop();
};


enum class ServerKind { Bio, Select, Poll, Epoll };
class Server{
    using V = std::variant<BioServer, SelectServer, PollServer, EpollServer>;
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