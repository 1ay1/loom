#include "../../include/loom/http/server.hpp"
#include "../../include/loom/http/request.hpp"
#include "../../include/loom/http/response.hpp"

#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
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

    // ── ServerSocket typestate transitions ────────────────────────
    //
    // Each transition is a linear implication (A ⊸ B): it consumes
    // the source state and produces the target. The && qualifier makes
    // this explicit — the method can only be called on rvalues.

    auto create_server_socket() -> ServerSocket<Unbound>
    {
        int fd = socket(AF_INET6, SOCK_STREAM, 0);
        if (fd < 0)
            throw std::runtime_error("socket() failed");

        int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        int v6only = 0;
        setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only));

        return ServerSocket<Unbound>{FileDescriptor{fd}};
    }

    auto ServerSocket<Unbound>::bind(int port) && -> ServerSocket<Bound>
    {
        sockaddr_in6 addr{};
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(static_cast<uint16_t>(port));
        addr.sin6_addr = in6addr_any;

        if (::bind(fd_.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
            throw std::runtime_error("bind() failed");

        return ServerSocket<Bound>{std::move(fd_)};
    }

    auto ServerSocket<Bound>::listen() && -> ServerSocket<Listening>
    {
        if (::listen(fd_.get(), SOMAXCONN) < 0)
            throw std::runtime_error("listen() failed");

        set_nonblocking(fd_.get());

        return ServerSocket<Listening>{std::move(fd_)};
    }

    // ── HttpServer ────────────────────────────────────────────────

    HttpServer::HttpServer(ServerSocket<Listening> socket)
        : server_fd_(std::move(socket.fd_))
    {}

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
        epoll_ctl(epoll_fd_.get(), EPOLL_CTL_DEL, fd, nullptr);
        connections_.erase(fd);
        close(fd);
    }

    void HttpServer::accept_connections()
    {
        while (true)
        {
            int client_fd = accept(server_fd_.get(), nullptr, nullptr);
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
            epoll_ctl(epoll_fd_.get(), EPOLL_CTL_ADD, client_fd, &ev);

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

        // Connection must be in ReadPhase — the variant prevents
        // reading data while a write is in progress
        auto* read = std::get_if<ReadPhase>(&conn.phase);
        if (!read) return;

        char buf[8192];
        while (true)
        {
            ssize_t n = ::read(fd, buf, sizeof(buf));
            if (n > 0)
            {
                read->buf.append(buf, static_cast<size_t>(n));

                if (read->buf.size() > MAX_REQUEST_SIZE)
                {
                    start_write_owned(fd,
                        DynamicResponse::bad_request().serialize(false));
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

        auto* read = std::get_if<ReadPhase>(&conn.phase);
        if (!read) return;

        auto header_end = read->buf.find("\r\n\r\n");
        if (header_end == std::string::npos)
            return;

        // Check Content-Length for body completion
        std::string_view buf_view(read->buf);
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
            if (read->buf.size() - (header_end + 4) < content_length)
                return;
        }

        // Parse: returns variant<HttpRequest, ParseError>
        auto result = parse_request(read->buf);

        if (!std::holds_alternative<HttpRequest>(result))
        {
            start_write_owned(fd,
                DynamicResponse::bad_request().serialize(false));
            conn.keep_alive = false;
            conn.phase.emplace<ReadPhase>();
            return;
        }

        auto& request = std::get<HttpRequest>(result);
        conn.keep_alive = request.keep_alive();

        // Dispatch returns variant<DynamicResponse, PrebuiltResponse>
        // std::visit handles both cases — exhaustive, no dead fields
        auto response = dispatch_(request);

        std::visit(overloaded{
            [&](DynamicResponse& r) {
                start_write_owned(fd, r.serialize(conn.keep_alive));
            },
            [&](PrebuiltResponse& r) {
                start_write_view(fd, std::move(r.owner), r.data, r.len);
            }
        }, response);
    }

    void HttpServer::start_write_owned(int fd, std::string data)
    {
        auto it = connections_.find(fd);
        if (it == connections_.end())
            return;

        auto& conn = it->second;

        // Transition: ReadPhase → WritePhase (OwnedWrite variant)
        auto& write = conn.phase.emplace<WritePhase>(
            OwnedWrite{std::move(data)});

        handle_writable(fd);
        (void)write;
    }

    void HttpServer::start_write_view(int fd, std::shared_ptr<const void> owner,
                                       const char* data, size_t len)
    {
        auto it = connections_.find(fd);
        if (it == connections_.end())
            return;

        auto& conn = it->second;

        // Transition: ReadPhase → WritePhase (ViewWrite variant)
        auto& write = conn.phase.emplace<WritePhase>(
            ViewWrite{std::move(owner), data, len});

        handle_writable(fd);
        (void)write;
    }

    void HttpServer::handle_writable(int fd)
    {
        auto it = connections_.find(fd);
        if (it == connections_.end())
            return;

        auto& conn = it->second;

        auto* write = std::get_if<WritePhase>(&conn.phase);
        if (!write) return;

        // std::visit over OwnedWrite | ViewWrite — both share the
        // cursor/remaining/advance interface via structural typing
        while (true)
        {
            auto remaining = std::visit(
                [](auto& w) { return w.remaining(); }, *write);
            if (remaining == 0) break;

            auto cursor = std::visit(
                [](auto& w) { return w.cursor(); }, *write);

            ssize_t n = ::write(fd, cursor, remaining);

            if (n > 0)
            {
                std::visit(
                    [n](auto& w) { w.advance(static_cast<size_t>(n)); },
                    *write);
                continue;
            }

            if (n < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    epoll_event ev{};
                    ev.events = EPOLLOUT | EPOLLET;
                    ev.data.fd = fd;
                    epoll_ctl(epoll_fd_.get(), EPOLL_CTL_MOD, fd, &ev);
                    return;
                }
                if (errno == EINTR)
                    continue;
            }

            close_connection(fd);
            return;
        }

        // Write complete
        conn.last_activity_ms = now_ms();

        if (!conn.keep_alive)
        {
            close_connection(fd);
            return;
        }

        // Transition: WritePhase → ReadPhase
        conn.phase.emplace<ReadPhase>();

        epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = fd;
        epoll_ctl(epoll_fd_.get(), EPOLL_CTL_MOD, fd, &ev);

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
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
        signal(SIGPIPE, SIG_IGN);

        epoll_fd_ = FileDescriptor{epoll_create1(0)};
        if (!epoll_fd_)
        {
            perror("epoll_create1");
            return;
        }

        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = server_fd_.get();
        epoll_ctl(epoll_fd_.get(), EPOLL_CTL_ADD, server_fd_.get(), &ev);

        running_.store(true);
        g_running.store(true);

        std::cout << "Listening (single-threaded event loop)\n";

        epoll_event events[MAX_EVENTS];
        int64_t last_reap = now_ms();

        while (running_.load() && g_running.load())
        {
            int nfds = epoll_wait(epoll_fd_.get(), events, MAX_EVENTS, 1000);

            if (nfds < 0)
            {
                if (errno == EINTR) continue;
                perror("epoll_wait");
                break;
            }

            for (int i = 0; i < nfds; ++i)
            {
                int fd = events[i].data.fd;

                if (fd == server_fd_.get())
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
    }
}
