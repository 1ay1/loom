#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace loom
{
    enum class HttpMethod { GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS, UNKNOWN };

    HttpMethod parse_method(const std::string& method);

    struct HttpRequest
    {
        HttpMethod method = HttpMethod::GET;
        std::string path;
        std::string query;
        std::vector<std::string> params;
        std::unordered_map<std::string, std::string> headers;
        std::string body;

        std::string header(const std::string& key) const;
        bool keep_alive() const;
    };

    bool parse_request(const std::string& raw, HttpRequest& request);
}
