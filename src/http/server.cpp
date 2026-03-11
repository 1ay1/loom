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

        bind(server_fd, (sockaddr*)&addr, sizeof(addr));

        listen(server_fd, 100);

        std::cout << "Server listening on port " << port_ << "\n";

        while (true)
        {
            int client = accept(server_fd, nullptr, nullptr);
            char buffer[4096];

            int n = read(client, buffer, sizeof(buffer));
            std::string request(buffer , n);

            std::string path = "/";

            auto start = request.find("GET ");
            if(start != std::string::npos)
            {
                start += 4;
                auto end = request.find(" ", start);
                path = request.substr(start, end - start);
            }

            std::string body = handler_(path);

            std::string response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: " + std::to_string(body.size()) + "\r\n"
                "\r\n" +
                body;

            write(client, response.c_str(), response.size());

            close(client);
        }
    }
}
