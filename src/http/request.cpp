#include "../../include/loom/http/request.hpp"

#include <cctype>

namespace loom
{
    static constexpr HttpMethod parse_method(std::string_view m) noexcept
    {
        if (m == "GET")     return HttpMethod::GET;
        if (m == "POST")    return HttpMethod::POST;
        if (m == "PUT")     return HttpMethod::PUT;
        if (m == "DELETE")  return HttpMethod::DELETE;
        if (m == "PATCH")   return HttpMethod::PATCH;
        if (m == "HEAD")    return HttpMethod::HEAD;
        if (m == "OPTIONS") return HttpMethod::OPTIONS;
        return HttpMethod::UNKNOWN;
    }

    std::string_view HttpRequest::header(std::string_view key) const
    {
        for (const auto& h : headers_)
            if (h.key == key) return h.value;
        return {};
    }

    bool HttpRequest::keep_alive() const
    {
        auto v = header("connection");
        // Fast case-insensitive check for "close"
        if (v.size() == 5)
        {
            return !((v[0] | 0x20) == 'c' && (v[1] | 0x20) == 'l' &&
                     (v[2] | 0x20) == 'o' && (v[3] | 0x20) == 's' &&
                     (v[4] | 0x20) == 'e');
        }
        return true;
    }

    auto parse_request(std::string& raw) -> ParseResult
    {
        HttpRequest request;
        std::string_view sv(raw);

        auto header_end = sv.find("\r\n\r\n");
        if (header_end == std::string_view::npos)
            return ParseError{"incomplete headers"};

        auto first_line_end = sv.find("\r\n");
        if (first_line_end == std::string_view::npos)
            return ParseError{"no request line"};

        auto method_end = sv.find(' ');
        if (method_end == std::string_view::npos || method_end >= first_line_end)
            return ParseError{"malformed request line"};

        request.method = parse_method(sv.substr(0, method_end));

        auto path_start = method_end + 1;
        auto path_end = sv.find(' ', path_start);
        if (path_end == std::string_view::npos || path_end >= first_line_end)
            return ParseError{"malformed URI"};

        auto uri = sv.substr(path_start, path_end - path_start);
        auto qpos = uri.find('?');
        if (qpos != std::string_view::npos)
        {
            request.path = uri.substr(0, qpos);
            request.query = uri.substr(qpos + 1);
        }
        else
        {
            request.path = uri;
        }

        // Headers — lowercase keys in-place in the raw buffer
        size_t pos = first_line_end + 2;
        while (pos < header_end)
        {
            auto line_end = sv.find("\r\n", pos);
            if (line_end == std::string_view::npos || line_end > header_end) break;

            auto colon = sv.find(':', pos);
            if (colon != std::string_view::npos && colon < line_end)
            {
                for (size_t i = pos; i < colon; ++i)
                    raw[i] = static_cast<char>(
                        std::tolower(static_cast<unsigned char>(raw[i])));

                auto val_start = colon + 1;
                while (val_start < line_end && raw[val_start] == ' ')
                    ++val_start;

                request.headers_.push_back({
                    sv.substr(pos, colon - pos),
                    sv.substr(val_start, line_end - val_start)
                });
            }
            pos = line_end + 2;
        }

        // Body
        for (const auto& h : request.headers_)
        {
            if (h.key == "content-length")
            {
                size_t cl = 0;
                for (char c : h.value)
                    if (c >= '0' && c <= '9') cl = cl * 10 + (c - '0');
                size_t body_start = header_end + 4;
                if (sv.size() - body_start < cl)
                    return ParseError{"incomplete body"};
                request.body = sv.substr(body_start, cl);
                break;
            }
        }

        return request;
    }
}
