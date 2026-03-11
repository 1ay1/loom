#pragma once

#include <string>
#include <functional>
#include <vector>

namespace loom
{
    using Params = std::vector<std::string>;
    using RouteHandler = std::function<std::string(const Params&)>;

    class Router
    {
        public:
        void add(std::string pattern, RouteHandler handler);
        std::string route(const std::string& path);

        private:

        struct Route
        {
            std::string pattern;
            RouteHandler handler;
        };

        std::vector<Route> routes_;

        bool match(const std::string& pattern,
        const std::string& path,
        Params& params);
    };
}
