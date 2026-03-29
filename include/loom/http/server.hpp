#pragma once

// ═══════════════════════════════════════════════════════════════════════
//  server.hpp — type-theoretic HTTP server
//
//  Three type-theoretic patterns are at work here:
//
//  1. ServerSocket typestate: Unbound → Bound → Listening. Each state
//     is a type. Transitions consume the source (&&) and produce the
//     target. Protocol violations are type errors. (Part 4)
//
//  2. Connection phase as a sum type: std::variant<ReadPhase, WritePhase>.
//     Each variant carries exactly the data relevant to that phase.
//     No dead fields. (Part 2)
//
//  3. FileDescriptor as an affine type: move-only, auto-closing.
//     Prevents double-close and leak. (Part 8)
// ═══════════════════════════════════════════════════════════════════════

#include "request.hpp"
#include "response.hpp"
#include "../core/fd.hpp"

#include <atomic>
#include <functional>
#include <string>
#include <unordered_map>
#include <memory>
#include <variant>
#include <cstddef>
#include <stdexcept>

namespace loom
{

// ── Server socket lifecycle typestate ─────────────────────────
//
// Three states, three types. The transitions are methods qualified
// with && — they consume *this by move, making the source state
// unavailable after the transition fires.
//
//   auto socket = create_server_socket()   // Unbound
//       .bind(8080)                        // Bound
//       .listen();                         // Listening
//
// Calling .listen() on an Unbound socket is a type error.
// Calling .bind() on a Listening socket is a type error.

struct Unbound {};
struct Bound {};
struct Listening {};

template<typename State>
class ServerSocket;

template<>
class ServerSocket<Unbound>
{
    FileDescriptor fd_;
    friend class ServerSocket<Bound>;
public:
    explicit ServerSocket(FileDescriptor fd) : fd_(std::move(fd)) {}

    // Unbound ⊸ Bound: bind to a port, consuming the unbound socket
    [[nodiscard]] auto bind(int port) && -> ServerSocket<Bound>;
};

template<>
class ServerSocket<Bound>
{
    FileDescriptor fd_;
    friend class ServerSocket<Listening>;
public:
    explicit ServerSocket(FileDescriptor fd) : fd_(std::move(fd)) {}

    // Bound ⊸ Listening: start accepting, consuming the bound socket
    [[nodiscard]] auto listen() && -> ServerSocket<Listening>;
};

template<>
class ServerSocket<Listening>
{
    FileDescriptor fd_;
    friend class HttpServer;
public:
    explicit ServerSocket(FileDescriptor fd) : fd_(std::move(fd)) {}
    int fd() const noexcept { return fd_.get(); }
};

// Factory: creates an IPv6 dual-stack socket in the Unbound state
[[nodiscard]] auto create_server_socket() -> ServerSocket<Unbound>;

// ── Connection phase: sum type ────────────────────────────────
//
// A connection is either reading a request or writing a response.
// Each variant carries exactly the data relevant to that phase —
// no dead fields, no invalid combinations. (Part 2: algebraic types)

struct ReadPhase
{
    std::string buf;
};

// The write phase is itself a sum type: the response data is EITHER
// owned (serialized on the fly) OR borrowed (a view into the cache).
// Each variant carries exactly its own data. No dead fields.

struct OwnedWrite
{
    std::string data;
    size_t offset = 0;

    const char* cursor() const noexcept { return data.data() + offset; }
    size_t remaining() const noexcept { return data.size() - offset; }
    void advance(size_t n) noexcept { offset += n; }
};

struct ViewWrite
{
    std::shared_ptr<const void> owner;
    const char* data;
    size_t len;
    size_t offset = 0;

    const char* cursor() const noexcept { return data + offset; }
    size_t remaining() const noexcept { return len - offset; }
    void advance(size_t n) noexcept { offset += n; }
};

using WritePhase = std::variant<OwnedWrite, ViewWrite>;

struct Connection
{
    std::variant<ReadPhase, WritePhase> phase{ReadPhase{}};
    bool keep_alive = true;
    int64_t last_activity_ms = 0;
};

// ── HTTP Server ───────────────────────────────────────────────

class HttpServer
{
public:
    using Dispatch = std::function<HttpResponse(HttpRequest&)>;

    // Takes a Listening socket — typestate guarantees setup is complete
    explicit HttpServer(ServerSocket<Listening> socket);

    void set_dispatch(Dispatch fn);
    void run();
    void stop();

private:
    static constexpr size_t MAX_REQUEST_SIZE = 1024 * 1024;
    static constexpr int MAX_EVENTS = 256;
    static constexpr int KEEPALIVE_TIMEOUT_MS = 5000;

    FileDescriptor server_fd_;
    FileDescriptor epoll_fd_;
    Dispatch dispatch_;
    std::atomic<bool> running_{false};
    std::unordered_map<int, Connection> connections_;

    void accept_connections();
    void handle_readable(int fd);
    void handle_writable(int fd);
    void process_request(int fd);
    void start_write_owned(int fd, std::string data);
    void start_write_view(int fd, std::shared_ptr<const void> owner,
                          const char* data, size_t len);
    void close_connection(int fd);
    void reap_idle_connections();
    int64_t now_ms() const;
};

} // namespace loom
