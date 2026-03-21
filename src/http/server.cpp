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
#include <chrono>
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

    HttpServer::HttpServer(int port)
        : port_(port) {}

    Router& HttpServer::router()
    {
        return router_;
    }

    void HttpServer::stop()
    {
        running_.store(false);
    }

    int64_t HttpServer::now_ms() const
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    void HttpServer::close_connection(int fd)
    {
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
        connections_.erase(fd);
        close(fd);
    }

    void HttpServer::accept_connections()
    {
        while (true)
        {
            int client_fd = accept(server_fd_, nullptr, nullptr);
            if (client_fd < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                if (errno == EINTR)
                    continue;
                perror("accept");
                break;
            }

            set_nonblocking(client_fd);

            int flag = 1;
            setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

            epoll_event ev{};
            ev.events = EPOLLIN | EPOLLET;
            ev.data.fd = client_fd;
            epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &ev);

            connections_[client_fd] = Connection{{}, {}, 0, true, now_ms()};
        }
    }

    void HttpServer::handle_readable(int fd)
    {
        auto it = connections_.find(fd);
        if (it == connections_.end())
            return;

        auto& conn = it->second;
        conn.last_activity_ms = now_ms();

        char buf[8192];
        while (true)
        {
            ssize_t n = read(fd, buf, sizeof(buf));
            if (n > 0)
            {
                conn.read_buf.append(buf, static_cast<size_t>(n));

                if (conn.read_buf.size() > MAX_REQUEST_SIZE)
                {
                    auto resp = HttpResponse::bad_request().serialize(false);
                    start_write(fd, resp);
                    conn.keep_alive = false;
                    return;
                }
                continue;
            }

            if (n == 0)
            {
                close_connection(fd);
                return;
            }

            // n < 0
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            if (errno == EINTR)
                continue;
            close_connection(fd);
            return;
        }

        // Try to process complete requests from the buffer
        process_request(fd);
    }

    void HttpServer::process_request(int fd)
    {
        auto it = connections_.find(fd);
        if (it == connections_.end())
            return;

        auto& conn = it->second;

        // Check if we have a complete request (headers end with \r\n\r\n)
        auto header_end = conn.read_buf.find("\r\n\r\n");
        if (header_end == std::string::npos)
            return;

        // Check Content-Length for body completion
        auto cl_pos = conn.read_buf.find("Content-Length:");
        if (cl_pos == std::string::npos)
            cl_pos = conn.read_buf.find("content-length:");

        if (cl_pos != std::string::npos && cl_pos < header_end)
        {
            auto val_start = cl_pos + 15;
            auto val_end = conn.read_buf.find("\r\n", val_start);
            size_t content_length = std::stoul(
                conn.read_buf.substr(val_start, val_end - val_start));
            size_t body_start = header_end + 4;

            if (conn.read_buf.size() - body_start < content_length)
                return; // Body not yet complete
        }

        HttpRequest request;
        if (!parse_request(conn.read_buf, request))
        {
            auto resp = HttpResponse::bad_request().serialize(false);
            start_write(fd, resp);
            conn.keep_alive = false;
            conn.read_buf.clear();
            return;
        }

        conn.keep_alive = request.keep_alive();

        auto response = router_.route(request);
        auto raw = response.serialize(conn.keep_alive);

        // Clear the consumed request from the buffer
        conn.read_buf.clear();

        start_write(fd, raw);
    }

    void HttpServer::start_write(int fd, const std::string& data)
    {
        auto it = connections_.find(fd);
        if (it == connections_.end())
            return;

        auto& conn = it->second;
        conn.write_buf = data;
        conn.write_offset = 0;

        // Try to write immediately
        handle_writable(fd);
    }

    void HttpServer::handle_writable(int fd)
    {
        auto it = connections_.find(fd);
        if (it == connections_.end())
            return;

        auto& conn = it->second;

        while (conn.write_offset < conn.write_buf.size())
        {
            ssize_t n = write(fd,
                conn.write_buf.c_str() + conn.write_offset,
                conn.write_buf.size() - conn.write_offset);

            if (n > 0)
            {
                conn.write_offset += static_cast<size_t>(n);
                continue;
            }

            if (n < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    // Switch to waiting for writable
                    epoll_event ev{};
                    ev.events = EPOLLOUT | EPOLLET;
                    ev.data.fd = fd;
                    epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
                    return;
                }
                if (errno == EINTR)
                    continue;
            }

            // Write error or n == 0
            close_connection(fd);
            return;
        }

        // Write complete
        conn.write_buf.clear();
        conn.write_offset = 0;
        conn.last_activity_ms = now_ms();

        if (!conn.keep_alive)
        {
            close_connection(fd);
            return;
        }

        // Switch back to reading for next request
        epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = fd;
        epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
    }

    void HttpServer::reap_idle_connections()
    {
        auto now = now_ms();
        std::vector<int> to_close;

        for (auto& [fd, conn] : connections_)
        {
            if (now - conn.last_activity_ms > KEEPALIVE_TIMEOUT_MS)
                to_close.push_back(fd);
        }

        for (int fd : to_close)
            close_connection(fd);
    }

    void HttpServer::run()
    {
        server_fd_ = socket(AF_INET6, SOCK_STREAM, 0);
        if (server_fd_ < 0)
        {
            perror("socket");
            return;
        }

        int opt = 1;
        setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        int v6only = 0;
        setsockopt(server_fd_, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only));

        sockaddr_in6 addr{};
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(static_cast<uint16_t>(port_));
        addr.sin6_addr = in6addr_any;

        if (bind(server_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        {
            perror("bind");
            close(server_fd_);
            return;
        }

        if (listen(server_fd_, SOMAXCONN) < 0)
        {
            perror("listen");
            close(server_fd_);
            return;
        }

        set_nonblocking(server_fd_);

        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
        signal(SIGPIPE, SIG_IGN);

        epoll_fd_ = epoll_create1(0);
        if (epoll_fd_ < 0)
        {
            perror("epoll_create1");
            close(server_fd_);
            return;
        }

        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = server_fd_;
        epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, server_fd_, &ev);

        running_.store(true);
        g_running.store(true);

        std::cout << "Listening on port " << port_
                  << " (single-threaded event loop)\n";

        epoll_event events[MAX_EVENTS];
        int64_t last_reap = now_ms();

        while (running_.load() && g_running.load())
        {
            int nfds = epoll_wait(epoll_fd_, events, MAX_EVENTS, 1000);

            if (nfds < 0)
            {
                if (errno == EINTR) continue;
                perror("epoll_wait");
                break;
            }

            for (int i = 0; i < nfds; ++i)
            {
                int fd = events[i].data.fd;

                if (fd == server_fd_)
                {
                    accept_connections();
                    continue;
                }

                if (events[i].events & (EPOLLERR | EPOLLHUP))
                {
                    close_connection(fd);
                    continue;
                }

                if (events[i].events & EPOLLIN)
                    handle_readable(fd);

                if (events[i].events & EPOLLOUT)
                    handle_writable(fd);
            }

            // Reap idle keep-alive connections every second
            auto now = now_ms();
            if (now - last_reap > 1000)
            {
                reap_idle_connections();
                last_reap = now;
            }
        }

        std::cout << "Shutting down...\n";

        // Close all active connections
        for (auto& [fd, _] : connections_)
            close(fd);
        connections_.clear();

        close(epoll_fd_);
        close(server_fd_);
    }
}
