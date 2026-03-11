#pragma once

#include "router.hpp"
#include <string>
#include <functional>

namespace loom
{
    using Handler = std::function<std::string(const std::string& path)>;

    class HttpServer
    {
      public:
      explicit HttpServer(int port);
      Router& router();
      void run();

      private:
      int port_;
      Router router_;
    };
}
