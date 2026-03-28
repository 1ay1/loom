#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <utility>

namespace loom
{
    struct HttpResponse
    {
        int status = 200;
        std::vector<std::pair<std::string, std::string>> headers;
        std::string body;

        // Pre-serialized wire data — when set, serialize() is bypassed entirely.
        // wire_owner_ keeps the backing memory alive (typically a SiteCache snapshot).
        std::shared_ptr<const void> wire_owner_;
        const char* wire_data_ = nullptr;
        size_t wire_len_ = 0;

        bool is_prebuilt() const noexcept { return wire_data_ != nullptr; }

        static HttpResponse prebuilt(std::shared_ptr<const void> owner,
                                      const std::string& wire)
        {
            HttpResponse r;
            r.wire_owner_ = std::move(owner);
            r.wire_data_ = wire.data();
            r.wire_len_ = wire.size();
            return r;
        }

        static HttpResponse ok(std::string body, const std::string& content_type = "text/html");
        static HttpResponse not_found(std::string body = "");
        static HttpResponse bad_request(std::string body = "");
        static HttpResponse method_not_allowed();
        static HttpResponse internal_error(std::string body = "");

        std::string serialize(bool keep_alive) const;
    };
}
