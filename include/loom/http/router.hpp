#pragma once

#include "request.hpp"
#include "response.hpp"

#include <string>
#include <functional>
#include <vector>
#include <unordered_map>
#include <memory>

namespace loom
{
    using RouteHandler = std::function<HttpResponse(const HttpRequest&)>;

    class Router
    {
    public:
        void get(const std::string& pattern, RouteHandler handler);
        void post(const std::string& pattern, RouteHandler handler);
        void put(const std::string& pattern, RouteHandler handler);
        void del(const std::string& pattern, RouteHandler handler);

        HttpResponse route(HttpRequest& request) const;

    private:
        struct TrieNode
        {
            std::unordered_map<std::string, std::unique_ptr<TrieNode>> children;
            std::unique_ptr<TrieNode> param_child;
            std::string param_name;
            std::unordered_map<HttpMethod, RouteHandler> handlers;
        };

        TrieNode root_;

        void add_route(HttpMethod method, const std::string& pattern, RouteHandler handler);
        static std::vector<std::string> split_path(const std::string& path);
    };
}
