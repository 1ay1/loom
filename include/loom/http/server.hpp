#pragma once

#include "router.hpp"
#include "thread_pool.hpp"

#include <atomic>
#include <cstddef>

namespace loom
{
    class HttpServer
    {
    public:
        explicit HttpServer(int port, size_t num_threads = 8);

        Router& router();
        void run();
        void stop();

    private:
        int port_;
        Router router_;
        ThreadPool pool_;
        std::atomic<bool> running_{false};

        void handle_connection(int client_fd);
    };
}
