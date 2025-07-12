#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <mutex>

// 简单的日志类
class Logger {
public:
    template<typename... Args>
    static void log(Args&&... args) {
        std::lock_guard<std::mutex> lock(mutex_);
        (std::cout << ... << std::forward<Args>(args)) << std::endl;
    }
private:
    static std::mutex mutex_;
};

std::mutex Logger::mutex_;

// 计时器类
class Timer {
public:
    Timer() : start_(std::chrono::steady_clock::now()) {}
    
    template<typename Duration = std::chrono::milliseconds>
    auto elapsed() const {
        auto end = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<Duration>(end - start_).count();
    }
private:
    std::chrono::steady_clock::time_point start_;
};

class BenchmarkClient {
public:
    struct Config {
        std::string host = "127.0.0.1";
        uint16_t port = 18081;
        int num_clients = 100;
        int messages_per_client = 10;
        int message_interval_ms = 100;
    };
    
    struct Stats {
        std::atomic<int> successful_connections{0};
        std::atomic<int> failed_connections{0};
        std::atomic<int> successful_messages{0};
        std::atomic<int> failed_messages{0};
        std::atomic<long long> total_bytes_sent{0};
        std::atomic<long long> total_bytes_received{0};
    };
    
    BenchmarkClient(const Config& config) : config_(config) {}
    
    void run() {
        Logger::log("Starting benchmark with ", config_.num_clients, " clients, ",
                   config_.messages_per_client, " messages each");
        Logger::log("Target: ", config_.host, ":", config_.port);
        
        Timer timer;
        
        // 创建客户端线程
        std::vector<std::thread> threads;
        threads.reserve(config_.num_clients);
        
        for (int i = 0; i < config_.num_clients; ++i) {
            threads.emplace_back([this, i]() {
                run_client(i);
            });
        }
        
        // 等待所有线程完成
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto elapsed_ms = timer.elapsed();
        
        // 输出统计结果
        print_results(elapsed_ms);
    }
    
private:
    void run_client(int client_id) {
        // 创建socket
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            stats_.failed_connections++;
            return;
        }
        
        // 连接服务器
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(config_.port);
        
        if (inet_pton(AF_INET, config_.host.c_str(), &addr.sin_addr) <= 0) {
            close(sock);
            stats_.failed_connections++;
            return;
        }
        
        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close(sock);
            stats_.failed_connections++;
            return;
        }
        
        stats_.successful_connections++;
        
        // 发送消息
        for (int i = 0; i < config_.messages_per_client; ++i) {
            std::string message = "Hello from client " + std::to_string(client_id) + 
                                 " message " + std::to_string(i + 1) + "\n";
            
            ssize_t sent = send(sock, message.c_str(), message.length(), 0);
            if (sent > 0) {
                stats_.successful_messages++;
                stats_.total_bytes_sent += sent;
                
                // 接收响应
                char buffer[1024];
                ssize_t received = recv(sock, buffer, sizeof(buffer) - 1, 0);
                if (received > 0) {
                    stats_.total_bytes_received += received;
                } else {
                    stats_.failed_messages++;
                }
            } else {
                stats_.failed_messages++;
            }
            
            // 间隔
            if (config_.message_interval_ms > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(config_.message_interval_ms));
            }
        }
        
        close(sock);
    }
    
    void print_results(long long elapsed_ms) {
        Logger::log("\n=== Benchmark Results ===");
        Logger::log("Duration: ", elapsed_ms, "ms");
        Logger::log("Connections - Success: ", stats_.successful_connections.load(),
                   ", Failed: ", stats_.failed_connections.load());
        Logger::log("Messages - Success: ", stats_.successful_messages.load(),
                   ", Failed: ", stats_.failed_messages.load());
        Logger::log("Bytes - Sent: ", stats_.total_bytes_sent.load(),
                   ", Received: ", stats_.total_bytes_received.load());
        
        if (elapsed_ms > 0) {
            double messages_per_sec = (stats_.successful_messages.load() * 1000.0) / elapsed_ms;
            double mbps_sent = (stats_.total_bytes_sent.load() * 8.0 / 1024 / 1024 * 1000) / elapsed_ms;
            double mbps_received = (stats_.total_bytes_received.load() * 8.0 / 1024 / 1024 * 1000) / elapsed_ms;
            
            Logger::log("Throughput: ", std::fixed, std::setprecision(2),
                       messages_per_sec, " msg/s");
            Logger::log("Bandwidth - Sent: ", std::fixed, std::setprecision(2),
                       mbps_sent, " Mbps, Received: ", mbps_received, " Mbps");
        }
    }
    
    Config config_;
    Stats stats_;
};

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n"
              << "Options:\n"
              << "  -h, --host HOST        Server host (default: 127.0.0.1)\n"
              << "  -p, --port PORT        Server port (default: 18081)\n"
              << "  -c, --clients NUM      Number of clients (default: 100)\n"
              << "  -m, --messages NUM     Messages per client (default: 10)\n"
              << "  -i, --interval MS      Message interval in ms (default: 100)\n"
              << "  --help                 Show this help\n\n"
              << "Examples:\n"
              << "  " << program_name << " -c 50 -m 20\n"
              << "  " << program_name << " -h 192.168.1.100 -p 8080 -c 200\n";
}

int main(int argc, char* argv[]) {
    BenchmarkClient::Config config;
    
    // 简单的参数解析
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--host" || arg == "-h") {
            if (++i < argc) config.host = argv[i];
        } else if (arg == "--port" || arg == "-p") {
            if (++i < argc) config.port = static_cast<uint16_t>(std::stoi(argv[i]));
        } else if (arg == "--clients" || arg == "-c") {
            if (++i < argc) config.num_clients = std::stoi(argv[i]);
        } else if (arg == "--messages" || arg == "-m") {
            if (++i < argc) config.messages_per_client = std::stoi(argv[i]);
        } else if (arg == "--interval" || arg == "-i") {
            if (++i < argc) config.message_interval_ms = std::stoi(argv[i]);
        }
    }
    
    BenchmarkClient client(config);
    client.run();
    
    return 0;
} 