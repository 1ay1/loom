#pragma once

#include "request.hpp"
#include "response.hpp"

#include <string>
#include <string_view>
#include <functional>
#include <vector>
#include <unordered_map>
#include <memory>

namespace loom
{
    using RouteHandler = std::function<HttpResponse(const HttpRequest&)>;

    // Transparent hash — allows string_view lookups in string-keyed maps
    struct StringHash
    {
        using is_transparent = void;
        size_t operator()(std::string_view sv) const noexcept
        { return std::hash<std::string_view>{}(sv); }
    };

    class Router
    {
    public:
        void get(const std::string& pattern, RouteHandler handler);
        void post(const std::string& pattern, RouteHandler handler);
        void put(const std::string& pattern, RouteHandler handler);
        void del(const std::string& pattern, RouteHandler handler);

        void set_fallback(RouteHandler handler);

        HttpResponse route(HttpRequest& request) const;

    private:
        struct TrieNode
        {
            std::unordered_map<std::string, std::unique_ptr<TrieNode>,
                               StringHash, std::equal_to<>> children;
            std::unique_ptr<TrieNode> param_child;
            std::string param_name;
            std::unordered_map<HttpMethod, RouteHandler> handlers;
        };

        TrieNode root_;
        RouteHandler fallback_;

        void add_route(HttpMethod method, const std::string& pattern, RouteHandler handler);
        static std::vector<std::string_view> split_path(std::string_view path);
    };
}
