#pragma once

#include "request.hpp"
#include "response.hpp"

#include <atomic>
#include <functional>
#include <string>
#include <unordered_map>
#include <memory>
#include <cstddef>

namespace loom
{
    class HttpServer
    {
    public:
        using Dispatch = std::function<HttpResponse(HttpRequest&)>;

        explicit HttpServer(int port);

        // Set the dispatch function (typically a compiled route table)
        void set_dispatch(Dispatch fn);

        void run();
        void stop();

    private:
        static constexpr size_t MAX_REQUEST_SIZE = 1024 * 1024;
        static constexpr int MAX_EVENTS = 256;
        static constexpr int KEEPALIVE_TIMEOUT_MS = 5000;

        struct Connection
        {
            std::string read_buf;

            // Write state: either owned data or a view into shared cache data
            std::string write_owned;
            std::shared_ptr<const void> write_ref;
            const char* write_ptr = nullptr;
            size_t write_len = 0;
            size_t write_offset = 0;

            bool keep_alive = true;
            int64_t last_activity_ms = 0;
        };

        int port_;
        int server_fd_ = -1;
        int epoll_fd_ = -1;
        Dispatch dispatch_;
        std::atomic<bool> running_{false};
        std::unordered_map<int, Connection> connections_;

        void accept_connections();
        void handle_readable(int fd);
        void handle_writable(int fd);
        void process_request(int fd);
        void start_write_owned(int fd, std::string data);
        void start_write_view(int fd, std::shared_ptr<const void> owner,
                              const char* data, size_t len);
        void close_connection(int fd);
        void reap_idle_connections();
        int64_t now_ms() const;
    };
}
