#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

namespace loom
{
    enum class HttpMethod : uint8_t { GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS, UNKNOWN };

    struct HttpRequest
    {
        HttpMethod method = HttpMethod::GET;
        std::string_view path;
        std::string_view query;
        std::vector<std::string_view> params;
        std::string_view body;

        struct Header { std::string_view key; std::string_view value; };
        std::vector<Header> headers_;

        // Key must be lowercase — stored keys are lowercased during parsing.
        std::string_view header(std::string_view key) const;
        bool keep_alive() const;
    };

    // Parses an HTTP request. Modifies raw in-place (lowercases header keys).
    // The HttpRequest contains string_views into raw — raw must outlive the request.
    bool parse_request(std::string& raw, HttpRequest& request);
}
