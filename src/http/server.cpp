#include "../../include/loom/http/server.hpp"
#include "../../include/loom/http/request.hpp"
#include "../../include/loom/http/response.hpp"

#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <csignal>
#include <cerrno>
#include <iostream>

namespace loom
{
    static std::atomic<bool> g_running{true};

    static void signal_handler(int)
    {
        g_running.store(false);
    }

    static void set_nonblocking(int fd)
    {
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    HttpServer::HttpServer(int port, size_t num_threads)
        : port_(port), pool_(num_threads) {}

    Router& HttpServer::router()
    {
        return router_;
    }

    void HttpServer::stop()
    {
        running_.store(false);
    }

    void HttpServer::handle_connection(int client_fd)
    {
        int flag = 1;
        setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

        timeval tv{.tv_sec = 0, .tv_usec = 500000}; // 500ms recv timeout
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        std::string buffer;
        char read_buf[8192];
        bool keep_alive = true;
        int idle_rounds = 0;
        constexpr int max_idle_rounds = 4; // 4 x 500ms = 2s keep-alive timeout

        while (keep_alive && running_.load())
        {
            buffer.clear();
            bool request_complete = false;
            size_t header_end_pos = std::string::npos;

            while (!request_complete)
            {
                ssize_t n = read(client_fd, read_buf, sizeof(read_buf));
                if (n < 0)
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        if (!running_.load() || ++idle_rounds >= max_idle_rounds)
                        {
                            close(client_fd);
                            return;
                        }
                        continue;
                    }
                    if (errno == EINTR) continue;
                    close(client_fd);
                    return;
                }
                if (n == 0)
                {
                    close(client_fd);
                    return;
                }
                idle_rounds = 0;

                buffer.append(read_buf, static_cast<size_t>(n));

                if (header_end_pos == std::string::npos)
                    header_end_pos = buffer.find("\r\n\r\n");

                if (header_end_pos != std::string::npos)
                {
                    auto cl_pos = buffer.find("content-length:");
                    if (cl_pos == std::string::npos)
                        cl_pos = buffer.find("Content-Length:");

                    if (cl_pos != std::string::npos && cl_pos < header_end_pos)
                    {
                        auto val_start = cl_pos + 15;
                        auto val_end = buffer.find("\r\n", val_start);
                        size_t content_length = std::stoul(
                            buffer.substr(val_start, val_end - val_start));
                        size_t body_start = header_end_pos + 4;

                        if (buffer.size() - body_start >= content_length)
                            request_complete = true;
                    }
                    else
                    {
                        request_complete = true;
                    }
                }

                if (buffer.size() > 1024 * 1024)
                {
                    auto resp = HttpResponse::bad_request().serialize(false);
                    write(client_fd, resp.c_str(), resp.size());
                    close(client_fd);
                    return;
                }
            }

            HttpRequest request;
            if (!parse_request(buffer, request))
            {
                auto resp = HttpResponse::bad_request().serialize(false);
                write(client_fd, resp.c_str(), resp.size());
                close(client_fd);
                return;
            }

            keep_alive = request.keep_alive();

            auto response = router_.route(request);
            auto raw = response.serialize(keep_alive);

            size_t total = 0;
            while (total < raw.size())
            {
                ssize_t written = write(client_fd,
                    raw.c_str() + total, raw.size() - total);
                if (written <= 0)
                {
                    close(client_fd);
                    return;
                }
                total += static_cast<size_t>(written);
            }
        }

        close(client_fd);
    }

    void HttpServer::run()
    {
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0)
        {
            perror("socket");
            return;
        }

        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<uint16_t>(port_));
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        {
            perror("bind");
            close(server_fd);
            return;
        }

        if (listen(server_fd, SOMAXCONN) < 0)
        {
            perror("listen");
            close(server_fd);
            return;
        }

        set_nonblocking(server_fd);

        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
        signal(SIGPIPE, SIG_IGN);

        int epoll_fd = epoll_create1(0);
        if (epoll_fd < 0)
        {
            perror("epoll_create1");
            close(server_fd);
            return;
        }

        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = server_fd;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);

        running_.store(true);
        g_running.store(true);

        std::cout << "Listening on port " << port_
                  << " (" << pool_.size() << " workers)\n";

        epoll_event events[64];

        while (running_.load() && g_running.load())
        {
            int nfds = epoll_wait(epoll_fd, events, 64, 500);

            if (nfds < 0)
            {
                if (errno == EINTR) continue;
                perror("epoll_wait");
                break;
            }

            for (int i = 0; i < nfds; ++i)
            {
                if (events[i].data.fd == server_fd)
                {
                    while (true)
                    {
                        int client_fd = accept(server_fd, nullptr, nullptr);
                        if (client_fd < 0)
                        {
                            if (errno == EAGAIN || errno == EWOULDBLOCK)
                                break;
                            if (errno == EINTR)
                                continue;
                            perror("accept");
                            break;
                        }

                        pool_.enqueue([this, client_fd] {
                            handle_connection(client_fd);
                        });
                    }
                }
            }
        }

        std::cout << "Shutting down...\n";
        pool_.shutdown();
        close(epoll_fd);
        close(server_fd);
    }
}
