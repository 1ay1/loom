#pragma once

#include <string>
#include <functional>

namespace loom
{
    using Handler = std::function<std::string(const std::string& path)>;

    class HttpServer
    {
      public:
      explicit HttpServer(int port);
      void set_handler(Handler handler);
      void run();

      private:
      int port_;
      Handler handler_;
    };
}
