#pragma once

#include "router.hpp"

#include <atomic>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstddef>

namespace loom
{
    class HttpServer
    {
    public:
        explicit HttpServer(int port);

        Router& router();
        void run();
        void stop();

    private:
        static constexpr size_t MAX_REQUEST_SIZE = 1024 * 1024;
        static constexpr int MAX_EVENTS = 256;
        static constexpr int KEEPALIVE_TIMEOUT_MS = 5000;

        struct Connection
        {
            std::string read_buf;
            std::string write_buf;
            size_t write_offset = 0;
            bool keep_alive = true;
            int64_t last_activity_ms = 0;
        };

        int port_;
        int server_fd_ = -1;
        int epoll_fd_ = -1;
        Router router_;
        std::atomic<bool> running_{false};
        std::unordered_map<int, Connection> connections_;

        void accept_connections();
        void handle_readable(int fd);
        void handle_writable(int fd);
        void process_request(int fd);
        void start_write(int fd, const std::string& data);
        void close_connection(int fd);
        void reap_idle_connections();
        int64_t now_ms() const;
    };
}
