#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/tcp_sink.h"

// Server that listens for a single connection, processes one message, then exits.
void tcp_server_single(int port) {
    int server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return;
    }
    int opt = 1;
    if (::setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        ::close(server_fd);
        return;
    }
    sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (::bind(server_fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0) {
        perror("bind");
        ::close(server_fd);
        return;
    }
    if (::listen(server_fd, 1) < 0) {
        perror("listen");
        ::close(server_fd);
        return;
    }
    std::cout << "Server listening on port " << port << std::endl;

    int client_fd = ::accept(server_fd, nullptr, nullptr);
    if (client_fd < 0) {
        perror("accept");
        ::close(server_fd);
        return;
    }
    std::cout << "Server: Client connected" << std::endl;

    char buffer[1024] = {0};
    ssize_t n = ::read(client_fd, buffer, sizeof(buffer));
    if (n > 0) {
        std::cout << "Server received: " << std::string(buffer, n) << std::endl;
    } else if (n < 0) {
        perror("read");
    }

    ::close(client_fd);
    ::close(server_fd);
    std::cout << "Server: Connection closed, server exiting" << std::endl;
}

int main() {
    const int port = 12345;

    // Start a server that will accept one connection and then exit.
    std::thread server_thread(tcp_server_single, port);
    std::this_thread::sleep_for(std::chrono::seconds(1));  // give server time to start

    // Create a logger that connects to the server with a 5-second timeout.
    spdlog::sinks::tcp_sink_config config("10.255.255.1", port, 5);
    auto tcpSink = std::make_shared<spdlog::sinks::tcp_sink_mt>(config);
    auto logger = std::make_shared<spdlog::logger>("TCP Logger", tcpSink);
    logger->set_level(spdlog::level::err);

    // First log: should connect successfully and send the message.
    try {
        logger->error("First message: connection established");
    } catch (const spdlog::spdlog_ex &ex) {
        std::cerr << "Exception during first log: " << ex.what() << std::endl;
    }

    server_thread.join();  // server is now down

    std::cout << "Server is down. Now attempting to reconnect..." << std::endl;

    // Second log: since the connection is no longer valid, a reconnect is attempted.
    // Because there is no server, the connect() will eventually time out and throw.
    try {
        logger->error("Second message: expecting timeout error");
    } catch (const spdlog::spdlog_ex &ex) {
        std::cerr << "Timeout error caught: " << ex.what() << std::endl;
    }

    return 0;
}
