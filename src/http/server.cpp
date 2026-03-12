#include <atomic>
#include <csignal> // for signal, SIGINT, SIGTERM
#include <sys/select.h> // for select, fd_set, timeval
#include <errno.h> // for EINTR

#include "../../include/loom/http/http.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

// Global flag to control the server loop
std::atomic<bool> server_running_{true};

// Signal handler function
void signal_handler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received. Shutting down gracefully...\n";
    server_running_.store(false); // Set the flag to false to exit the loop
}

namespace loom
{
    HttpServer::HttpServer(int port):
    port_(port) {}


    Router& HttpServer::router()
    {
        return router_;
    }

    void HttpServer::run()
    {
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) {
            perror("socket creation failed");
            return;
        }

        // Set SO_REUSEADDR option to allow restarting the server quickly
        int optval = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
            perror("setsockopt(SO_REUSEADDR) failed");
            // This is not critical enough to stop startup, but good to log.
        }

        sockaddr_in addr{};
        addr.sin_family  = AF_INET;
        addr.sin_port = htons(port_);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("bind failed");
            close(server_fd); // Ensure socket is closed on error
            return;
        }

        if (listen(server_fd, 100) < 0) {
            perror("listen failed");
            close(server_fd);
            return;
        }

        // Register signal handler for graceful shutdown
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);

        std::cout << "Server listening on port " << port_ << "\n";

        while (server_running_) // Check the running flag
        {
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(server_fd, &readfds); // Add server socket to the set

            // Timeout for select. Set to a short duration to make shutdown responsive.
            timeval tv;
            tv.tv_sec = 1; // Check every second
            tv.tv_usec = 0;

            // Wait for activity on server_fd or timeout
            int activity = select(server_fd + 1, &readfds, NULL, NULL, &tv);

            if (activity < 0) {
                // Error in select
                if (errno != EINTR) { // EINTR is expected if interrupted by a signal
                    perror("select failed");
                }
                // If EINTR, it means a signal was received, and server_running_ should be false.
                // The loop condition will handle termination.
                continue;
            }

            if (activity > 0) {
                if (FD_ISSET(server_fd, &readfds)) {
                    // A new connection is ready to be accepted
                    int client = accept(server_fd, nullptr, nullptr);
                    if (client < 0) {
                        // If accept fails but server_running_ is still true, it might be an error
                        // or a signal interruption that returned early from accept.
                        // If interrupted by signal, server_running_ will be false and loop will exit.
                        if (server_running_) {
                            perror("accept failed");
                        }
                        continue; // Try to accept next connection or exit if server_running_ is false
                    }
                    char buffer[4096];

                    int n = read(client, buffer, sizeof(buffer));
                    if (n < 0) {
                        perror("read failed");
                        close(client);
                        continue;
                    }
                    if (n == 0) {
                        // Client closed connection gracefully
                        close(client);
                        continue;
                    }

                    // Basic request parsing
                    std::string request(buffer, n);
                    std::string path = "/"; // Default path
                    std::string method = "GET"; // Default method

                    auto method_end = request.find(' ');
                    if (method_end != std::string::npos) {
                        method = request.substr(0, method_end);
                        auto path_start = method_end + 1;
                        auto path_end = request.find(' ', path_start);
                        if (path_end != std::string::npos) {
                            path = request.substr(path_start, path_end - path_start);
                        } else {
                            // Malformed request (e.g., "GET /")
                            // Default to root path if path is not clearly delimited.
                        }
                    } else {
                        // Could not parse method, malformed request. Send 400 Bad Request.
                        std::string error_body = "<html><body><h1>400 Bad Request</h1></body></html>";
                        std::string error_response =
                            "HTTP/1.1 400 Bad Request\r\n"
                            "Content-Type: text/html\r\n"
                            "Content-Length: " + std::to_string(error_body.size()) + "\r\n"
                            "\r\n" +
                            error_body;
                        ssize_t written = write(client, error_response.c_str(), error_response.size());
                        if (written < 0) perror("write failed");
                        close(client);
                        continue;
                    }

                    std::string body;
                    std::string status_line = "HTTP/1.1 200 OK\r\n"; // Default success

                    if (method == "GET") {
                        try {
                            body = router_.route(path);
                        } catch (const std::exception& e) {
                            std::cerr << "Handler exception: " << e.what() << std::endl;
                            status_line = "HTTP/1.1 500 Internal Server Error\r\n";
                            body = "<html><body><h1>500 Internal Server Error</h1></body></html>";
                        }
                    } else {
                        // Unsupported method
                        status_line = "HTTP/1.1 405 Method Not Allowed\r\n";
                        body = "<html><body><h1>405 Method Not Allowed</h1></body></html>";
                    }

                    std::string response =
                        status_line +
                        "Content-Type: text/html\r\n" // Still hardcoded, could be dynamic
                        "Content-Length: " + std::to_string(body.size()) + "\r\n"
                        "\r\n" +
                        body;

                    ssize_t bytes_written = write(client, response.c_str(), response.size());
                    if (bytes_written < 0) {
                        perror("write failed");
                    } else if (static_cast<size_t>(bytes_written) != response.size()) {
                        std::cerr << "Warning: Not all data written to client. Expected " << response.size() << ", wrote " << bytes_written << std::endl;
                    }

                    close(client); // Ensure client socket is closed
                }
            }
            // If activity == 0 (timeout), the loop just continues and re-checks server_running_
        }

        std::cout << "Server stopped. Closing socket.\n";
        close(server_fd); // Cleanup the server socket
    }
}
