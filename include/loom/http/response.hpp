#pragma once

#include <string>
#include <unordered_map>

namespace loom
{
    struct HttpResponse
    {
        int status = 200;
        std::unordered_map<std::string, std::string> headers;
        std::string body;

        static HttpResponse ok(std::string body, const std::string& content_type = "text/html");
        static HttpResponse not_found(std::string body = "");
        static HttpResponse bad_request(std::string body = "");
        static HttpResponse method_not_allowed();
        static HttpResponse internal_error(std::string body = "");

        std::string serialize(bool keep_alive) const;
    };
}
