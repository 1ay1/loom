#include "../../include/loom/http/request.hpp"

#include <algorithm>

namespace loom
{
    HttpMethod parse_method(const std::string& method)
    {
        if (method == "GET")     return HttpMethod::GET;
        if (method == "POST")    return HttpMethod::POST;
        if (method == "PUT")     return HttpMethod::PUT;
        if (method == "DELETE")  return HttpMethod::DELETE;
        if (method == "PATCH")   return HttpMethod::PATCH;
        if (method == "HEAD")    return HttpMethod::HEAD;
        if (method == "OPTIONS") return HttpMethod::OPTIONS;
        return HttpMethod::UNKNOWN;
    }

    std::string HttpRequest::header(const std::string& key) const
    {
        std::string lower;
        lower.reserve(key.size());
        for (char c : key)
            lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        auto it = headers.find(lower);
        return it != headers.end() ? it->second : "";
    }

    bool HttpRequest::keep_alive() const
    {
        std::string conn = header("connection");
        for (auto& c : conn)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return conn != "close";
    }

    bool parse_request(const std::string& raw, HttpRequest& request)
    {
        auto header_end = raw.find("\r\n\r\n");
        if (header_end == std::string::npos)
            return false;

        // Request line
        auto first_line_end = raw.find("\r\n");
        if (first_line_end == std::string::npos)
            return false;

        auto method_end = raw.find(' ');
        if (method_end == std::string::npos || method_end >= first_line_end)
            return false;

        request.method = parse_method(raw.substr(0, method_end));

        auto path_start = method_end + 1;
        auto path_end = raw.find(' ', path_start);
        if (path_end == std::string::npos || path_end >= first_line_end)
            return false;

        std::string uri = raw.substr(path_start, path_end - path_start);

        auto query_pos = uri.find('?');
        if (query_pos != std::string::npos)
        {
            request.path = uri.substr(0, query_pos);
            request.query = uri.substr(query_pos + 1);
        }
        else
        {
            request.path = uri;
        }

        // Headers
        size_t pos = first_line_end + 2;
        while (pos < header_end)
        {
            auto line_end = raw.find("\r\n", pos);
            if (line_end == std::string::npos || line_end > header_end)
                break;

            auto colon = raw.find(':', pos);
            if (colon != std::string::npos && colon < line_end)
            {
                std::string key = raw.substr(pos, colon - pos);
                for (auto& c : key)
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

                auto val_start = colon + 1;
                while (val_start < line_end && raw[val_start] == ' ')
                    ++val_start;

                request.headers[key] = raw.substr(val_start, line_end - val_start);
            }

            pos = line_end + 2;
        }

        // Body
        auto cl_it = request.headers.find("content-length");
        if (cl_it != request.headers.end())
        {
            size_t content_length = std::stoul(cl_it->second);
            size_t body_start = header_end + 4;

            if (raw.size() - body_start < content_length)
                return false;

            request.body = raw.substr(body_start, content_length);
        }

        return true;
    }
}
