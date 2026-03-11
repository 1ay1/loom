#include "../../include/loom/http/server.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

namespace loom
{
    HttpServer::HttpServer(int port):
    port_(port) {}


    void HttpServer::set_handler(Handler handler)
    {
        handler_ = std::move(handler);
    }

    void HttpServer::run()
    {
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);

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

        std::cout << "Server listening on port " << port_ << "\n";

        while (true)
        {
            int client = accept(server_fd, nullptr, nullptr);
            if (client < 0) {
                perror("accept failed");
                continue; // Try to accept next connection
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
                    body = handler_(path);
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
}
