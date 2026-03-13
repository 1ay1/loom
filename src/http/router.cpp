#include "../../include/loom/http/router.hpp"

#include <sstream>

namespace loom
{
    std::vector<std::string> Router::split_path(const std::string& path)
    {
        std::vector<std::string> segments;
        std::istringstream stream(path);
        std::string segment;

        while (std::getline(stream, segment, '/'))
        {
            if (!segment.empty())
                segments.push_back(segment);
        }

        return segments;
    }

    void Router::add_route(HttpMethod method, const std::string& pattern, RouteHandler handler)
    {
        auto segments = split_path(pattern);
        TrieNode* node = &root_;

        for (const auto& seg : segments)
        {
            if (seg.starts_with(':'))
            {
                if (!node->param_child)
                {
                    node->param_child = std::make_unique<TrieNode>();
                    node->param_child->param_name = seg.substr(1);
                }
                node = node->param_child.get();
            }
            else
            {
                auto& child = node->children[seg];
                if (!child)
                    child = std::make_unique<TrieNode>();
                node = child.get();
            }
        }

        node->handlers[method] = std::move(handler);
    }

    void Router::get(const std::string& pattern, RouteHandler handler)
    {
        add_route(HttpMethod::GET, pattern, std::move(handler));
    }

    void Router::post(const std::string& pattern, RouteHandler handler)
    {
        add_route(HttpMethod::POST, pattern, std::move(handler));
    }

    void Router::put(const std::string& pattern, RouteHandler handler)
    {
        add_route(HttpMethod::PUT, pattern, std::move(handler));
    }

    void Router::del(const std::string& pattern, RouteHandler handler)
    {
        add_route(HttpMethod::DELETE, pattern, std::move(handler));
    }

    HttpResponse Router::route(HttpRequest& request) const
    {
        auto segments = split_path(request.path);
        const TrieNode* node = &root_;
        std::vector<std::string> params;

        for (const auto& seg : segments)
        {
            auto it = node->children.find(seg);
            if (it != node->children.end())
            {
                node = it->second.get();
                continue;
            }

            if (node->param_child)
            {
                params.push_back(seg);
                node = node->param_child.get();
                continue;
            }

            return HttpResponse::not_found();
        }

        auto handler_it = node->handlers.find(request.method);
        if (handler_it == node->handlers.end())
        {
            if (node->handlers.empty())
                return HttpResponse::not_found();
            return HttpResponse::method_not_allowed();
        }

        request.params = std::move(params);

        try
        {
            return handler_it->second(request);
        }
        catch (const std::exception&)
        {
            return HttpResponse::internal_error();
        }
    }
}
