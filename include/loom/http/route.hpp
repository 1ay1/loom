#pragma once

// Compile-time routing DSL.
//
// Routes are defined as types: the pattern is a non-type template parameter
// (C++20 NTTP), the method is an enum, and the handler is a callable.
// The compiler sees the entire route table as a single expression and
// generates an optimal dispatch chain — no trie, no hash map, no heap.
//
// Usage:
//
//   using namespace loom::route;
//   auto dispatch = compile(
//       get<"/">(index_handler),
//       get<"/post/:slug">(post_handler),
//       get<"/tag/:slug">(tag_handler),
//       fallback(not_found_handler)
//   );
//
//   HttpResponse resp = dispatch(request);

#include "request.hpp"
#include "response.hpp"

#include <string_view>
#include <tuple>
#include <utility>

namespace loom::route {

// ── Compile-time string literal (structural type for NTTPs) ──────

template<size_t N>
struct Lit {
    char buf[N]{};

    constexpr Lit(const char (&s)[N]) noexcept
    { for (size_t i = 0; i < N; ++i) buf[i] = s[i]; }

    constexpr std::string_view sv()     const noexcept { return {buf, N - 1}; }
    constexpr size_t            size()  const noexcept { return N - 1; }
    constexpr char              operator[](size_t i) const noexcept { return buf[i]; }
    constexpr bool              operator==(const Lit&) const = default;
};

template<size_t N> Lit(const char (&)[N]) -> Lit<N>;

// ── Compile-time pattern analysis ────────────────────────────────
//
// Given a pattern like "/post/:slug", determines at compile time:
//   - Whether it's static (no parameters) or parameterized
//   - The literal prefix before the first parameter ("/post/")
//   - How to match and extract at runtime with zero overhead

template<Lit P>
struct Traits {
    // True when the pattern has no ':' parameter
    static consteval bool is_static() noexcept
    {
        for (size_t i = 0; i < P.size(); ++i)
            if (P[i] == ':') return false;
        return true;
    }

    // Byte length of the literal prefix (everything before the ':')
    static consteval size_t prefix_len() noexcept
    {
        for (size_t i = 0; i < P.size(); ++i)
            if (P[i] == ':') return i;
        return P.size();
    }

    // Runtime match — branchless for static routes, prefix check for param routes
    static bool match(std::string_view path) noexcept
    {
        if constexpr (is_static())
        {
            return path == P.sv();
        }
        else
        {
            constexpr std::string_view prefix{P.buf, prefix_len()};
            return path.size() > prefix.size() && path.starts_with(prefix);
        }
    }

    // Extract the parameter value (only valid for non-static routes)
    static std::string_view param(std::string_view path) noexcept
    {
        return path.substr(prefix_len());
    }
};

// ── Route: (Method × Pattern × Handler) encoded at the type level ─

template<HttpMethod M, Lit P, typename H>
struct Route {
    H handler;

    constexpr explicit Route(H h) : handler(std::move(h)) {}

    // Try to match and dispatch. Returns true on match.
    bool try_dispatch(HttpRequest& req, HttpResponse& out) const
    {
        if (req.method != M) return false;
        if (!Traits<P>::match(req.path)) return false;

        if constexpr (!Traits<P>::is_static())
            req.params.emplace_back(Traits<P>::param(req.path));

        out = handler(req);
        return true;
    }
};

// ── DSL: get<"/path">(handler), post<"/path">(handler) ──────────

template<Lit P> struct get_t  {
    template<typename H> constexpr auto operator()(H h) const
    { return Route<HttpMethod::GET, P, H>{std::move(h)}; }
};
template<Lit P> struct post_t {
    template<typename H> constexpr auto operator()(H h) const
    { return Route<HttpMethod::POST, P, H>{std::move(h)}; }
};
template<Lit P> struct put_t  {
    template<typename H> constexpr auto operator()(H h) const
    { return Route<HttpMethod::PUT, P, H>{std::move(h)}; }
};
template<Lit P> struct del_t  {
    template<typename H> constexpr auto operator()(H h) const
    { return Route<HttpMethod::DELETE, P, H>{std::move(h)}; }
};

template<Lit P> inline constexpr get_t<P>  get{};
template<Lit P> inline constexpr post_t<P> post{};
template<Lit P> inline constexpr put_t<P>  put{};
template<Lit P> inline constexpr del_t<P>  del{};

// ── Fallback wrapper ─────────────────────────────────────────────

template<typename H>
struct Fallback {
    H handler;
    constexpr explicit Fallback(H h) : handler(std::move(h)) {}
};

template<typename H>
constexpr auto fallback(H h) { return Fallback<H>{std::move(h)}; }

// ── compile(): assemble routes into a zero-overhead dispatch fn ──
//
// The first argument must be a Fallback. Remaining arguments are Routes.
// Returns a callable: HttpResponse(HttpRequest&)
//
// The dispatch is a fold expression over the route tuple — the compiler
// sees every pattern as a constant and generates an optimal if-else chain.
// No vtable, no hash, no trie, no heap.

namespace detail {

template<typename FB, typename... Rs>
struct Compiled {
    std::tuple<Rs...> routes;
    FB fb;

    constexpr Compiled(FB fallback, Rs... rs)
        : routes(std::move(rs)...), fb(std::move(fallback)) {}

    HttpResponse operator()(HttpRequest& req) const
    {
        HttpResponse result;
        bool found = false;

        auto try_one = [&](const auto& route) {
            if (!found && route.try_dispatch(req, result))
                found = true;
        };

        std::apply([&](const auto&... route) {
            (try_one(route), ...);
        }, routes);

        return found ? result : fb.handler(req);
    }
};

} // namespace detail

template<typename H, typename... Routes>
constexpr auto compile(Fallback<H> fb, Routes... routes)
{
    return detail::Compiled<Fallback<H>, Routes...>(
        std::move(fb), std::move(routes)...);
}

} // namespace loom::route
