#pragma once

// ═══════════════════════════════════════════════════════════════════════
//  response.hpp — HTTP response as a sum type
//
//  A response is EITHER a dynamically-built response (status + headers +
//  body, serialized on demand) OR a prebuilt wire-format response (a
//  pointer into the cache, served zero-copy).
//
//  The old design had both sets of fields in one struct with an
//  is_prebuilt() runtime check — dead fields in every state. The
//  variant makes the two cases structurally distinct: each carries
//  exactly the data it needs, nothing more. (Part 2: algebraic types)
// ═══════════════════════════════════════════════════════════════════════

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <utility>
#include <variant>

namespace loom
{

// ── Alternative 1: dynamically-built response ─────────────────

struct DynamicResponse
{
    int status = 200;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;

    static DynamicResponse ok(std::string body,
                              const std::string& content_type = "text/html");
    static DynamicResponse not_found(std::string body = "");
    static DynamicResponse bad_request(std::string body = "");
    static DynamicResponse method_not_allowed();
    static DynamicResponse internal_error(std::string body = "");
    static DynamicResponse redirect(int code, std::string location);

    std::string serialize(bool keep_alive) const;
};

// ── Alternative 2: prebuilt wire-format response ──────────────
// Zero-copy: points directly into the SiteCache wire buffers.

struct PrebuiltResponse
{
    std::shared_ptr<const void> owner;
    const char* data;
    size_t len;
};

// ── HttpResponse: the sum type ────────────────────────────────
// DynamicResponse + PrebuiltResponse. The caller must handle both
// cases — std::visit enforces exhaustive matching.

using HttpResponse = std::variant<DynamicResponse, PrebuiltResponse>;

// Convenience: overloaded pattern for std::visit
template<typename... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<typename... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

} // namespace loom
