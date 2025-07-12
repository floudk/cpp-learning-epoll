#include "server.hpp"


Server* server = nullptr;

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " <server_type> [port]\n"
              << "  server_type: bio | select | poll | epoll\n"
              << "  port:        server port (default: 18081)\n\n"
              << "Examples:\n"
              << "  " << program_name << " bio\n"
              << "  " << program_name << " epoll 8080\n";
}


void signal_handler(int signal){
    Logger::info("Received signal ", signal, ", shutting down...");
    server->stop();
    std::exit(0);
}



int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    uint16_t port = (argc > 2) ? static_cast<uint16_t>(std::stoi(argv[2])) : 18081;
    Logger::info("Using port ", port, " for server ", argv[1]);
    ServerKind kind = ServerKind::Bio;
    if (std::string_view(argv[1]) == "select") {
        Logger::info("Using select server");
        kind = ServerKind::Select;
    } else if (std::string_view(argv[1]) == "poll") {
        kind = ServerKind::Poll;
    } else if (std::string_view(argv[1]) == "epoll") {
        kind = ServerKind::Epoll;
    }else if (std::string_view(argv[1]) == "iouring") {
        kind = ServerKind::IOUring;
    }
    Server the_server = Server::make(kind);
    server = &the_server;

    signal(SIGINT, signal_handler);// 2: ctrl+c
    signal(SIGTERM, signal_handler);// 15: kill

    Logger::info("Server ", server->get_name(), " started on port ", port);
    
    Timer timer;
    try{
        server->run(port);
    } catch (const std::exception& e){
        Logger::error("Server ", server->get_name(), " failed: ", e.what());
        return 1;
    }

    auto elapsed_ms = timer.elapsed();
    Logger::info("Server stopped after ", elapsed_ms, "ms");
    Logger::info("Final stats - Active: ", server->get_active_connections().load(),
                ", Total messages: ", server->get_total_messages().load());
    
    return 0;
}