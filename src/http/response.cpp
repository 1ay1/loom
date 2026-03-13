#include "../../include/loom/http/response.hpp"

namespace loom
{
    static const char* status_text(int status)
    {
        switch (status)
        {
            case 200: return "OK";
            case 301: return "Moved Permanently";
            case 304: return "Not Modified";
            case 400: return "Bad Request";
            case 404: return "Not Found";
            case 405: return "Method Not Allowed";
            case 413: return "Payload Too Large";
            case 500: return "Internal Server Error";
            default:  return "Unknown";
        }
    }

    HttpResponse HttpResponse::ok(std::string body, const std::string& content_type)
    {
        return {200, {{"Content-Type", content_type}}, std::move(body)};
    }

    HttpResponse HttpResponse::not_found(std::string body)
    {
        if (body.empty())
            body = "<html><body><h1>404 Not Found</h1></body></html>";
        return {404, {{"Content-Type", "text/html"}}, std::move(body)};
    }

    HttpResponse HttpResponse::bad_request(std::string body)
    {
        if (body.empty())
            body = "<html><body><h1>400 Bad Request</h1></body></html>";
        return {400, {{"Content-Type", "text/html"}}, std::move(body)};
    }

    HttpResponse HttpResponse::method_not_allowed()
    {
        return {405, {{"Content-Type", "text/html"}},
            "<html><body><h1>405 Method Not Allowed</h1></body></html>"};
    }

    HttpResponse HttpResponse::internal_error(std::string body)
    {
        if (body.empty())
            body = "<html><body><h1>500 Internal Server Error</h1></body></html>";
        return {500, {{"Content-Type", "text/html"}}, std::move(body)};
    }

    std::string HttpResponse::serialize(bool keep_alive) const
    {
        std::string out;
        out.reserve(256 + body.size());

        out += "HTTP/1.1 ";
        out += std::to_string(status);
        out += ' ';
        out += status_text(status);
        out += "\r\n";

        for (const auto& [key, value] : headers)
        {
            out += key;
            out += ": ";
            out += value;
            out += "\r\n";
        }

        if (headers.find("Content-Length") == headers.end())
        {
            out += "Content-Length: ";
            out += std::to_string(body.size());
            out += "\r\n";
        }

        out += "Connection: ";
        out += keep_alive ? "keep-alive" : "close";
        out += "\r\n\r\n";
        out += body;

        return out;
    }
}
