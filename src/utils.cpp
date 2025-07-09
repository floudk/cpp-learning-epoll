#include "utils.hpp"
bool set_reuseaddr(int fd){// allow address reuse
    int optval = 1;
    return ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == 0;
} 


std::string get_current_time(){
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms;
    return ss.str();
}

void print_stats(std::string_view server_name, int active_connections, long long total_messages){
    Logger::info("(", get_current_time(), ")", server_name, " - active connections: ", active_connections, " - total messages: ", total_messages);
}