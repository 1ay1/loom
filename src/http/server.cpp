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

    void HttpServer::set_dispatch(Dispatch fn)
    {
        dispatch_ = std::move(fn);
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

            auto& conn = connections_[client_fd];
            conn = Connection{};
            conn.last_activity_ms = now_ms();
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
                    start_write_owned(fd, HttpResponse::bad_request().serialize(false));
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

            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            if (errno == EINTR)
                continue;
            close_connection(fd);
            return;
        }

        process_request(fd);
    }

    void HttpServer::process_request(int fd)
    {
        auto it = connections_.find(fd);
        if (it == connections_.end())
            return;

        auto& conn = it->second;

        auto header_end = conn.read_buf.find("\r\n\r\n");
        if (header_end == std::string::npos)
            return;

        // Check Content-Length for body completion (string_view, no alloc)
        std::string_view buf_view(conn.read_buf);
        auto cl_pos = buf_view.find("Content-Length:");
        if (cl_pos == std::string_view::npos)
            cl_pos = buf_view.find("content-length:");

        if (cl_pos != std::string_view::npos && cl_pos < header_end)
        {
            auto val_start = cl_pos + 15;
            auto val_end = buf_view.find("\r\n", val_start);
            size_t content_length = 0;
            for (size_t i = val_start; i < val_end && i < buf_view.size(); ++i)
            {
                char c = buf_view[i];
                if (c >= '0' && c <= '9')
                    content_length = content_length * 10 + (c - '0');
            }
            if (conn.read_buf.size() - (header_end + 4) < content_length)
                return;
        }

        HttpRequest request;
        if (!parse_request(conn.read_buf, request))
        {
            start_write_owned(fd, HttpResponse::bad_request().serialize(false));
            conn.keep_alive = false;
            conn.read_buf.clear();
            return;
        }

        conn.keep_alive = request.keep_alive();

        auto response = dispatch_(request);

        conn.read_buf.clear();

        if (response.is_prebuilt())
        {
            start_write_view(fd, std::move(response.wire_owner_),
                             response.wire_data_, response.wire_len_);
        }
        else
        {
            start_write_owned(fd, response.serialize(conn.keep_alive));
        }
    }

    void HttpServer::start_write_owned(int fd, std::string data)
    {
        auto it = connections_.find(fd);
        if (it == connections_.end())
            return;

        auto& conn = it->second;
        conn.write_owned = std::move(data);
        conn.write_ref.reset();
        conn.write_ptr = conn.write_owned.data();
        conn.write_len = conn.write_owned.size();
        conn.write_offset = 0;

        handle_writable(fd);
    }

    void HttpServer::start_write_view(int fd, std::shared_ptr<const void> owner,
                                       const char* data, size_t len)
    {
        auto it = connections_.find(fd);
        if (it == connections_.end())
            return;

        auto& conn = it->second;
        conn.write_ref = std::move(owner);
        conn.write_owned.clear();
        conn.write_ptr = data;
        conn.write_len = len;
        conn.write_offset = 0;

        handle_writable(fd);
    }

    void HttpServer::handle_writable(int fd)
    {
        auto it = connections_.find(fd);
        if (it == connections_.end())
            return;

        auto& conn = it->second;

        while (conn.write_offset < conn.write_len)
        {
            ssize_t n = write(fd,
                conn.write_ptr + conn.write_offset,
                conn.write_len - conn.write_offset);

            if (n > 0)
            {
                conn.write_offset += static_cast<size_t>(n);
                continue;
            }

            if (n < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    epoll_event ev{};
                    ev.events = EPOLLOUT | EPOLLET;
                    ev.data.fd = fd;
                    epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
                    return;
                }
                if (errno == EINTR)
                    continue;
            }

            close_connection(fd);
            return;
        }

        conn.write_owned.clear();
        conn.write_ref.reset();
        conn.write_ptr = nullptr;
        conn.write_len = 0;
        conn.write_offset = 0;
        conn.last_activity_ms = now_ms();

        if (!conn.keep_alive)
        {
            close_connection(fd);
            return;
        }

        epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = fd;
        epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);

        handle_readable(fd);
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

            auto now = now_ms();
            if (now - last_reap > 1000)
            {
                reap_idle_connections();
                last_reap = now;
            }
        }

        std::cout << "Shutting down...\n";

        for (auto& [fd, _] : connections_)
            close(fd);
        connections_.clear();

        close(epoll_fd_);
        close(server_fd_);
    }
}
