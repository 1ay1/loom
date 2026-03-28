#include "../../include/loom/http/response.hpp"

namespace loom
{
    // Compile-time status line lookup — no std::to_string at runtime
    static constexpr std::string_view status_line(int status) noexcept
    {
        switch (status)
        {
            case 200: return "HTTP/1.1 200 OK\r\n";
            case 301: return "HTTP/1.1 301 Moved Permanently\r\n";
            case 302: return "HTTP/1.1 302 Found\r\n";
            case 304: return "HTTP/1.1 304 Not Modified\r\n";
            case 400: return "HTTP/1.1 400 Bad Request\r\n";
            case 404: return "HTTP/1.1 404 Not Found\r\n";
            case 405: return "HTTP/1.1 405 Method Not Allowed\r\n";
            case 413: return "HTTP/1.1 413 Payload Too Large\r\n";
            case 500: return "HTTP/1.1 500 Internal Server Error\r\n";
            default:  return "HTTP/1.1 200 OK\r\n";
        }
    }

    HttpResponse HttpResponse::ok(std::string body, const std::string& content_type)
    {
        HttpResponse r;
        r.status = 200;
        r.headers = {{"Content-Type", content_type}};
        r.body = std::move(body);
        return r;
    }

    HttpResponse HttpResponse::not_found(std::string body)
    {
        if (body.empty())
            body = "<html><body><h1>404 Not Found</h1></body></html>";
        HttpResponse r;
        r.status = 404;
        r.headers = {{"Content-Type", "text/html"}};
        r.body = std::move(body);
        return r;
    }

    HttpResponse HttpResponse::bad_request(std::string body)
    {
        if (body.empty())
            body = "<html><body><h1>400 Bad Request</h1></body></html>";
        HttpResponse r;
        r.status = 400;
        r.headers = {{"Content-Type", "text/html"}};
        r.body = std::move(body);
        return r;
    }

    HttpResponse HttpResponse::method_not_allowed()
    {
        HttpResponse r;
        r.status = 405;
        r.headers = {{"Content-Type", "text/html"}};
        r.body = "<html><body><h1>405 Method Not Allowed</h1></body></html>";
        return r;
    }

    HttpResponse HttpResponse::internal_error(std::string body)
    {
        if (body.empty())
            body = "<html><body><h1>500 Internal Server Error</h1></body></html>";
        HttpResponse r;
        r.status = 500;
        r.headers = {{"Content-Type", "text/html"}};
        r.body = std::move(body);
        return r;
    }

    std::string HttpResponse::serialize(bool keep_alive) const
    {
        std::string out;
        out.reserve(256 + body.size());

        auto sl = status_line(status);
        out.append(sl.data(), sl.size());

        for (const auto& [key, value] : headers)
        {
            out += key;
            out += ": ";
            out += value;
            out += "\r\n";
        }

        out += "Content-Length: ";
        out += std::to_string(body.size());
        out += "\r\nConnection: ";
        out += keep_alive ? "keep-alive" : "close";
        out += "\r\n\r\n";
        out += body;

        return out;
    }
}
