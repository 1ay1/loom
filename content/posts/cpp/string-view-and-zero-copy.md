---
title: "string_view and Zero-Copy Parsing — Why Allocations Are the Enemy"
date: 2025-09-05
slug: string-view-and-zero-copy
tags: cpp, string-view, performance, zero-copy, parsing
excerpt: Every allocation is a conversation with the operating system. string_view lets you avoid the conversation entirely.
---

In the ownership post, I said that `std::string` owns its data. That's true, and it's exactly the problem. When you pass a `string` to a function, even by `const&`, someone had to create that string in the first place — allocating heap memory, copying characters, managing a buffer. For a blog engine serving thousands of requests per second, those allocations add up.

`std::string_view` is the solution: a non-owning, read-only view into a character sequence. It's two things — a pointer and a length — and it never allocates.

## What string_view Is

A `string_view` is a window into someone else's string data:

```cpp
#include <string_view>

std::string owned = "Hello, World!";
std::string_view view = owned;  // points into owned's buffer

// view.data() == owned.data()
// view.size() == owned.size()
// No allocation. No copy.
```

It works with string literals too:

```cpp
std::string_view greeting = "Hello";  // points to static memory
// No std::string created. No heap allocation. Zero cost.
```

And with substrings — this is where the magic happens:

```cpp
std::string_view full = "GET /post/hello-world HTTP/1.1";
std::string_view method = full.substr(0, 3);        // "GET"
std::string_view path = full.substr(4, 22);          // "/post/hello-world"
```

`string_view::substr` doesn't allocate. It returns a new view — a different pointer and length — into the same underlying data. Compare this to `string::substr`, which allocates a new string and copies the characters.

## Loom's Zero-Copy HTTP Parser

This is where string_view earns its keep. Look at Loom's HttpRequest struct:

```cpp
struct HttpRequest {
    HttpMethod method = HttpMethod::GET;
    std::string_view path;
    std::string_view query;
    std::vector<std::string_view> params;
    std::string_view body;

    struct Header { std::string_view key; std::string_view value; };
    std::vector<Header> headers_;
};
```

Every field is a `string_view`. The path, query, body, header keys, header values — all of them are views into the raw request buffer. The parser doesn't copy a single byte:

```cpp
// Parses an HTTP request. Modifies raw in-place (lowercases header keys).
// The HttpRequest contains string_views into raw — raw must outlive the request.
bool parse_request(std::string& raw, HttpRequest& request);
```

Read that comment carefully: "raw must outlive the request." This is the fundamental contract of `string_view`. The view doesn't own the data, so the data must stay alive as long as the view exists.

The server ensures this by keeping the read buffer alive in the Connection struct:

```cpp
struct Connection {
    std::string read_buf;
    // ... other fields
};
```

The `read_buf` lives as long as the connection. The `HttpRequest` is parsed from `read_buf` and used within `process_request()`. By the time the request is done, the response is built and the read buffer can be cleared. The lifetimes are perfect.

This zero-copy design means parsing an HTTP request requires zero heap allocations beyond the initial socket read. No `new`, no `malloc`, no string construction. For a server handling thousands of requests per second, this is the difference between microseconds and milliseconds of latency.

## string_view Operations

`string_view` supports most read operations you'd expect:

```cpp
std::string_view path = "/post/hello-world";

bool starts = path.starts_with("/post/");  // true (C++20)
bool ends = path.ends_with("/");            // false (C++20)

auto slash = path.find('/', 1);            // 5
auto sub = path.substr(6);                 // "hello-world"
auto len = path.size();                    // 20
auto c = path[0];                          // '/'
bool empty = path.empty();                 // false
```

Loom's route matching uses these extensively:

```cpp
static bool match(std::string_view path) noexcept {
    if constexpr (is_static()) {
        return path == P.sv();  // exact match for static routes
    } else {
        constexpr std::string_view prefix{P.buf, prefix_len()};
        return path.size() > prefix.size() && path.starts_with(prefix);
    }
}

static std::string_view param(std::string_view path) noexcept {
    return path.substr(prefix_len());  // extract the parameter
}
```

The route `/post/:slug` has prefix `/post/`. Matching checks `starts_with`, and parameter extraction is a single `substr` — both zero-allocation operations.

## The Content-Length Parser

Look at this code from Loom's server:

```cpp
std::string_view buf_view(conn.read_buf);
auto cl_pos = buf_view.find("Content-Length:");
if (cl_pos == std::string_view::npos)
    cl_pos = buf_view.find("content-length:");

if (cl_pos != std::string_view::npos && cl_pos < header_end) {
    auto val_start = cl_pos + 15;
    auto val_end = buf_view.find("\r\n", val_start);
    size_t content_length = 0;
    for (size_t i = val_start; i < val_end && i < buf_view.size(); ++i) {
        char c = buf_view[i];
        if (c >= '0' && c <= '9')
            content_length = content_length * 10 + (c - '0');
    }
}
```

The entire Content-Length parsing happens on a `string_view` of the read buffer. No substrings are created. No integers are parsed via `std::stoi` (which would create a temporary `std::string`). The parser reads characters directly and builds the integer by hand. This is the zero-copy philosophy taken to its logical conclusion.

## Lifetime Dangers

The power of string_view comes with a responsibility: you must ensure the underlying data outlives the view. Here's how things go wrong:

```cpp
// DANGER: returning a view into a local
std::string_view bad() {
    std::string local = "hello";
    return local;  // local is destroyed — view points to freed memory
}

// DANGER: view into a temporary
std::string_view also_bad = std::string("temp");
// the temporary string is destroyed at the semicolon
// also_bad now points to freed memory
```

The compiler sometimes warns about these, but not always. The rule is simple: **never return a string_view to data that you created locally, and never store a string_view beyond the lifetime of its source.**

Safe patterns:

```cpp
// View into a string literal — lives forever
constexpr std::string_view CONTENT_TYPE = "text/html; charset=utf-8";

// View into a member variable — lives as long as the object
class Parser {
    std::string buffer_;
    std::string_view current_token() const { return /* view into buffer_ */; }
};

// View as a function parameter — lives for the duration of the call
void process(std::string_view path) {
    // path is valid for the entire function body
}
```

## constexpr string_view

Because `string_view` is just a pointer and a length, it can be `constexpr`:

```cpp
static constexpr std::string_view mime_type(std::string_view path) noexcept {
    auto dot = path.rfind('.');
    if (dot == std::string_view::npos) return "application/octet-stream";
    auto ext = path.substr(dot);

    if (ext == ".png")  return "image/png";
    if (ext == ".jpg")  return "image/jpeg";
    if (ext == ".css")  return "text/css";
    if (ext == ".js")   return "application/javascript";
    // ...
    return "application/octet-stream";
}
```

This is from Loom's main.cpp. The function can be evaluated at compile time if called with a constant argument. At runtime, it's a simple chain of comparisons with no allocation.

The route pattern literals use a similar approach:

```cpp
constexpr std::string_view sv() const noexcept { return {buf, N - 1}; }
```

The `Lit` struct's `sv()` method returns a constexpr string_view of its compile-time string. The entire route matching system operates on string_views — from pattern definition to runtime dispatch, not a single string is allocated.

## Transparent Hashing

One subtle issue: if you have an `unordered_map<string, Value>`, you can't look up by `string_view` without constructing a temporary string. C++20 fixes this with transparent hashing:

```cpp
struct StringHash {
    using is_transparent = void;  // enables heterogeneous lookup
    size_t operator()(std::string_view sv) const {
        return std::hash<std::string_view>{}(sv);
    }
};

std::unordered_map<std::string, Response, StringHash, std::equal_to<>> cache;
cache.find(string_view_key);  // no temporary string created
```

The `is_transparent` tag and `std::equal_to<>` tell the map to accept any type that's comparable to the key type. Now you can look up by `string_view` in a map keyed by `string`, without allocating a temporary `string` for the lookup.

## The Allocation Budget

Here's how I think about it: every allocation is a conversation with the operating system's memory allocator. That conversation takes time — typically 50-100 nanoseconds, sometimes much more if the allocator needs to ask the kernel for pages. In a hot path that runs thousands of times per second, those nanoseconds become milliseconds.

`string_view` eliminates allocations from read-only string operations. Loom's HTTP pipeline processes a request through parsing, routing, parameter extraction, and response lookup without a single allocation (aside from the initial socket read). The response is served from a pre-built cache, with the `shared_ptr` keeping the cache alive until all in-flight responses are done.

This is the real lesson: `string_view` is not just a convenience type. It's a design tool. When you design your interfaces around views instead of owned strings, you create a system where allocations happen once (when data enters the system) and everything else is views into that data. The HTTP request arrives as bytes in a buffer. Everything downstream is views into those bytes. The response is pre-serialized bytes in the cache. No intermediate representations, no format conversions, no allocations.

That's zero-copy design. And `string_view` is its foundation.

Next: templates. The feature that makes all of this zero-cost abstraction possible.
