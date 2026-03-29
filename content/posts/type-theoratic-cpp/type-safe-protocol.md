---
title: "Building a Type-Safe Protocol — The Grand Synthesis"
date: 2026-03-29
slug: type-safe-protocol
tags: [c++20, type-theory, typestate, protocols, compile-time, capstone]
excerpt: "We combine every technique from this series — algebraic types, phantom types, typestate, concepts, compile-time data, parametricity, linear logic, F-algebras — to build a compile-time verified protocol where violations are type errors, not runtime surprises."
---

This is the final post in the series. Over nine posts, we have built a vocabulary:

- [Algebraic types](/post/algebraic-data-types) to model domain data with semiring arithmetic
- [Phantom types](/post/phantom-types) to create zero-cost distinctions via parametricity
- [Typestate](/post/typestate-programming) to encode state machines grounded in linear logic
- [Concepts](/post/concepts-as-logic) as propositions in intuitionistic natural deduction
- [Compile-time data](/post/compile-time-data) for dependent types and staged computation
- [Parametricity](/post/parametricity) for free theorems and reasoning from type signatures
- [Substructural types](/post/substructural-types) for resource management via affine logic
- [Recursive types](/post/recursive-types-and-fixed-points) as fixed points with catamorphisms

Now we put them all together. We will build a type-safe HTTP client where the compiler verifies the entire request lifecycle. Violating any rule is a type error, not a runtime exception.

## The Protocol

```
Idle → Connected → HeadersSent → BodySent → ResponseReceived → Done
```

Constraints:
- You must set `Host` and `Content-Type` before sending
- You can only read the response after the full request is sent
- After reading, the connection must be closed
- Connections cannot be reused

In a traditional implementation, each constraint is a runtime check. In ours, each is a compile-time guarantee.

## Step 1: State Types (Typestate + Algebraic Types)

Each state is a type carrying exactly the data relevant to that state — a product type ([part 2](/post/algebraic-data-types)) whose fields are faithful to the domain:

```cpp
struct Idle {};

struct Connected {
    int socket_fd;
};

struct HeadersSent {
    int socket_fd;
    bool has_host;
    bool has_content_type;
};

struct BodySent {
    int socket_fd;
};

struct ResponseReceived {
    int socket_fd;
    int status_code;
    std::string body;
};
```

`Idle` has no socket — you have not connected. `Connected` has a socket but no header state. `ResponseReceived` has the full response. No dead fields. The cardinality of each type matches the domain.

Through the substructural lens ([part 8](/post/substructural-types)): each state value is *affine* — it can be consumed (moved into a transition) or discarded (destructor cleanup), but not copied. This prevents two parts of the code from holding the same connection in different states.

## Step 2: Phantom Tags for Header Requirements (Phantom Types + Parametricity)

We need the compiler to track which headers have been set. Two independent boolean axes — perfect for phantom types:

```cpp
struct NoHost {};
struct HasHost {};
struct NoCT {};
struct HasCT {};

template<typename HostState, typename CTState>
struct Headers {
    int socket_fd;
    std::vector<std::pair<std::string, std::string>> entries;
};
```

`Headers<NoHost, NoCT>` means no required headers set. `Headers<HasHost, HasCT>` means both set. Four distinct types, same runtime layout.

Through parametricity ([part 7](/post/parametricity)): functions generic in `HostState` and `CTState` cannot inspect the phantom tags. They must propagate them faithfully. `set_header<HS, CS>` (for optional headers) preserves whatever state the phantoms encode — parametricity *guarantees* it cannot corrupt the tags.

## Step 3: Transition Functions (Linear Logic)

Each transition consumes the current state and produces the next — the linear implication A ⊸ B from [part 4](/post/typestate-programming):

```cpp
auto connect(Idle, std::string_view host, int port) -> Headers<NoHost, NoCT> {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    // ... connect to host:port ...
    return Headers<NoHost, NoCT>{fd, {}};
}

template<typename CTState>
auto set_host(Headers<NoHost, CTState> h, std::string_view host)
    -> Headers<HasHost, CTState>
{
    h.entries.emplace_back("Host", std::string(host));
    return Headers<HasHost, CTState>{h.socket_fd, std::move(h.entries)};
}

template<typename HostState>
auto set_content_type(Headers<HostState, NoCT> h, std::string_view ct)
    -> Headers<HostState, HasCT>
{
    h.entries.emplace_back("Content-Type", std::string(ct));
    return Headers<HostState, HasCT>{h.socket_fd, std::move(h.entries)};
}

template<typename HS, typename CS>
auto set_header(Headers<HS, CS> h, std::string key, std::string value)
    -> Headers<HS, CS>
{
    h.entries.emplace_back(std::move(key), std::move(value));
    return h;
}
```

Notice the phantom type flow: `set_host` changes `NoHost → HasHost` but preserves `CTState`. `set_content_type` changes `NoCT → HasCT` but preserves `HostState`. Independent axes, independently tracked. And `set_host` only accepts `Headers<NoHost, _>` — setting Host twice is a type error.

Each function takes its input by value: `Idle` consumed by `connect`, `Headers<NoHost, _>` consumed by `set_host`. These are linear implications. The `std::move` makes the consumption explicit.

## Step 4: The Gate (Concepts as Logic)

Sending the body requires both headers. This is a concept — a proposition in intuitionistic logic ([part 5](/post/concepts-as-logic)):

```cpp
auto send_body(Headers<HasHost, HasCT> h, std::string_view method,
               std::string_view path, std::string_view body) -> BodySent
{
    std::string request;
    request += method; request += " "; request += path; request += " HTTP/1.0\r\n";
    for (const auto& [key, value] : h.entries) {
        request += key; request += ": "; request += value; request += "\r\n";
    }
    request += "Content-Length: ";
    request += std::to_string(body.size());
    request += "\r\n\r\n";
    request += body;

    ::write(h.socket_fd, request.data(), request.size());
    return BodySent{h.socket_fd};
}
```

The function signature IS the constraint: `Headers<HasHost, HasCT>`. This is conjunction in natural deduction — both `HasHost` AND `HasCT` must be proven (established by the respective set functions). The compiler checks the proof at the call site.

## Step 5: Response and Cleanup

```cpp
auto receive(BodySent s) -> ResponseReceived {
    // ... read response from s.socket_fd ...
    return ResponseReceived{s.socket_fd, status, std::move(body)};
}

auto close(ResponseReceived r) -> void {
    ::close(r.socket_fd);
}
```

Each function consumes its input by value. The protocol is linear — each state value is used exactly once.

## The Complete Usage

```cpp
auto idle = Idle{};
auto hdrs = connect(idle, "example.com", 80);
hdrs = set_host(std::move(hdrs), "example.com");
hdrs = set_content_type(std::move(hdrs), "application/json");
hdrs = set_header(std::move(hdrs), "Accept", "application/json");
auto sent = send_body(std::move(hdrs), "POST", "/api/data", R"({"key": "value"})");
auto resp = receive(std::move(sent));
std::cout << "Status: " << resp.status_code << "\n" << resp.body << "\n";
close(std::move(resp));
```

Each line advances the protocol by one step. The types change at each step.

## Protocol Violations Are Type Errors

```cpp
// Forgot Content-Type:
auto hdrs = connect(Idle{}, "example.com", 80);
hdrs = set_host(std::move(hdrs), "example.com");
send_body(std::move(hdrs), "GET", "/", "");
// ERROR: cannot convert Headers<HasHost, NoCT> to Headers<HasHost, HasCT>
```

```cpp
// Tried to receive before sending:
auto hdrs = connect(Idle{}, "example.com", 80);
receive(std::move(hdrs));
// ERROR: no matching function — receive() takes BodySent, not Headers<...>
```

```cpp
// Set Host twice:
auto hdrs = connect(Idle{}, "example.com", 80);
hdrs = set_host(std::move(hdrs), "example.com");
hdrs = set_host(std::move(hdrs), "other.com");
// ERROR: set_host() takes Headers<NoHost, _>, but hdrs is Headers<HasHost, _>
```

Every violation is a type error. The error messages name the specific state types.

## The Fluent Interface

```cpp
template<typename State>
class HttpClient {
    State state_;
public:
    explicit HttpClient(State s) : state_(std::move(s)) {}

    auto set_host(std::string_view host) &&
        -> HttpClient<Headers<HasHost, typename deduce_ct<State>::type>>
        requires is_headers_no_host<State>;

    auto set_content_type(std::string_view ct) &&
        -> HttpClient<Headers<typename deduce_host<State>::type, HasCT>>
        requires is_headers_no_ct<State>;

    auto send(std::string_view method, std::string_view path, std::string_view body) &&
        -> HttpClient<BodySent>
        requires std::same_as<State, Headers<HasHost, HasCT>>;

    auto receive() && -> HttpClient<ResponseReceived>
        requires std::same_as<State, BodySent>;

    auto response() const -> const ResponseReceived&
        requires std::same_as<State, ResponseReceived>;
};
```

Usage:

```cpp
auto resp = HttpClient(connect(Idle{}, "example.com", 80))
    .set_host("example.com")
    .set_content_type("application/json")
    .send("POST", "/api", R"({"key": "value"})")
    .receive()
    .response();
```

The `&&` qualifier on methods means they consume the client — each call produces a new `HttpClient` with a different state type.

## Adding Compile-Time Routes (Dependent Types)

Using the techniques from [part 6](/post/compile-time-data):

```cpp
template<Lit Method, Lit Path>
struct Endpoint {
    static constexpr auto method = Method.sv();
    static constexpr auto path = Path.sv();
};

using GetUsers = Endpoint<"GET", "/api/users">;
using CreateUser = Endpoint<"POST", "/api/users">;

template<typename E>
concept HasBody = (E::method == "POST" || E::method == "PUT" || E::method == "PATCH");

template<typename E>
auto request(Headers<HasHost, HasCT> h, std::string_view body) -> BodySent
    requires HasBody<E>
{
    return send_body(std::move(h), E::method, E::path, body);
}

template<typename E>
auto request(Headers<HasHost, HasCT> h) -> BodySent
    requires (!HasBody<E>)
{
    return send_body(std::move(h), E::method, E::path, "");
}
```

Endpoints are compile-time values (Pi types — types depending on values). The `HasBody` concept is a proposition evaluated at compile time on the endpoint's method string. A `request<GetUsers>(headers, "body")` fails — GET does not have a body.

## The Cost: Zero

Every phantom type is an empty struct. Every state type compiles to its data fields. Template instantiation produces the same machine code as hand-written C. The `std::move` calls compile away. Concept checks are compile-time only.

At `-O2`, the entire protocol compiles to the same instructions as a hand-written implementation with no type safety. The type machinery is scaffolding that vanishes in the binary.

## Comparison

| Property | Runtime Checks | Type-Safe Protocol |
|---|---|---|
| Missing required header | Runtime exception | Compile error |
| Wrong method order | Runtime exception | Compile error |
| Double close | Runtime guard | Type error (moved-from) |
| Receive before send | Runtime assertion | Type error |
| Discoverability | Requires tests | Immediate at compile time |
| Runtime cost | Check on every call | Zero |
| Documentation | Comments, docs | The types ARE the docs |

## The Design Recipe

1. **Enumerate the states.** Each state becomes a type with exactly the data relevant to that state. (Algebraic types — [part 2](/post/algebraic-data-types))

2. **Identify independent axes.** Use phantom types for boolean requirements that can be set independently. (Phantom types — [part 3](/post/phantom-types))

3. **Define transitions as consuming functions.** Each transition takes the source state by value (move) and returns the target state. (Typestate + linear logic — [part 4](/post/typestate-programming), [part 8](/post/substructural-types))

4. **Gate critical transitions with concepts.** Entry to irreversible operations requires a concept that encodes all prerequisites. (Concepts as logic — [part 5](/post/concepts-as-logic))

5. **Use compile-time data for static structure.** Route patterns, endpoint definitions, configuration known at build time. (Compile-time data — [part 6](/post/compile-time-data))

6. **Keep tag-generic code parametric.** Functions that do not need to inspect phantom tags should be generic in them — parametricity guarantees safety. (Parametricity — [part 7](/post/parametricity))

7. **Use the substructural hierarchy.** Move-only types for affine resources, `[[nodiscard]]` for must-use return values, deleted copy constructors to prevent aliasing. (Substructural types — [part 8](/post/substructural-types))

## Beyond HTTP

The patterns generalize to any sequential protocol:

- **Database transactions**: Begin → Execute* → Commit/Rollback
- **File I/O**: Open → Read/Write* → Close
- **Authentication**: Unauthenticated → Challenged → Authenticated/Denied
- **Build systems**: Configure → Compile → Link → Package
- **Payment processing**: Cart → Checkout → Payment → Confirmed/Failed
- **TLS handshake**: ClientHello → ServerHello → KeyExchange → Established

Any protocol with ordered steps and preconditions can be encoded this way.

## In Practice: Loom's Server

The HTTP client protocol above is a pedagogical construction. Loom itself is a real HTTP server — a static site engine that serves a blog with hot reloading, compile-time routing, and type-safe HTML rendering. Every technique from this series appears in Loom's actual codebase. Here is how they come together.

### Algebraic Types: Connection, Response, ParseResult

Loom models HTTP connections, responses, and parse results as sum types using `std::variant`.

**Connection phase** (`include/loom/http/server.hpp`): a connection is either reading a request or writing a response. Each phase carries exactly the data relevant to that phase:

```cpp
struct ReadPhase  { std::string buf; };

struct OwnedWrite { std::string data; size_t offset = 0; };
struct ViewWrite  { std::shared_ptr<const void> owner;
                    const char* data; size_t len; size_t offset = 0; };
using WritePhase = std::variant<OwnedWrite, ViewWrite>;

struct Connection
{
    std::variant<ReadPhase, WritePhase> phase{ReadPhase{}};
    bool keep_alive = true;
    int64_t last_activity_ms = 0;
};
```

The outer variant `ReadPhase + WritePhase` is the connection lifecycle. The inner variant `OwnedWrite + ViewWrite` is a nested sum type within the write phase: the response is either a dynamically serialized string (owned) or a zero-copy view into the cache (borrowed via `shared_ptr`). No dead fields. No `is_writing` boolean. The type encodes the state.

**HttpResponse** (`include/loom/http/response.hpp`): the response itself is a sum type:

```cpp
struct DynamicResponse
{
    int status = 200;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
};

struct PrebuiltResponse
{
    std::shared_ptr<const void> owner;
    const char* data;
    size_t len;
};

using HttpResponse = std::variant<DynamicResponse, PrebuiltResponse>;
```

A `DynamicResponse` is built at request time — the handler constructs status, headers, and body. A `PrebuiltResponse` is a pointer into the cache — the entire wire-format HTTP response was pre-serialized, and serving it is a single `write()` call. The variant forces the server to handle both cases:

```cpp
std::visit(overloaded{
    [&](const DynamicResponse& r) { start_write_owned(fd, r.serialize(keep_alive)); },
    [&](const PrebuiltResponse& r) { start_write_view(fd, r.owner, r.data, r.len); },
}, response);
```

The `overloaded` helper is itself a type-theoretic construction — an anonymous visitor built from a parameter pack of lambdas using aggregate inheritance:

```cpp
template<typename... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<typename... Ts>
overloaded(Ts...) -> overloaded<Ts...>;
```

**ParseResult** (`include/loom/http/request.hpp`): parsing an HTTP request produces either a valid request or an error:

```cpp
struct ParseError { std::string_view reason; };
using ParseResult = std::variant<HttpRequest, ParseError>;
```

No boolean return with an out-parameter. No exception. The variant is the sum type `HttpRequest + ParseError`, and the caller must handle both cases. This is the `Either` monad from functional programming, expressed as a C++ variant.

### Phantom Types: StrongType for Domain Safety

Loom uses `StrongType<T, Tag>` (`include/loom/core/strong_type.hpp`) to prevent confusion between string-typed domain values:

```cpp
template<typename T, typename Tag>
class StrongType
{
public:
    explicit StrongType(T value) : value_(std::move(value)) {}
    T get() const { return value_; }
    bool operator==(const StrongType& other) const { return value_ == other.value_; }
private:
    T value_;
};
```

The phantom `Tag` parameter creates distinct types for values that share the same runtime representation:

```cpp
using Slug    = StrongType<std::string, SlugTag>;
using Title   = StrongType<std::string, TitleTag>;
using PostId  = StrongType<std::string, PostIdTag>;
using Content = StrongType<std::string, ContentTag>;
```

A `Slug` and a `Title` are both strings at runtime. But the compiler treats them as unrelated types. Passing a `Title` where a `Slug` is expected is a compile error. The `Tag` is never stored, never inspected — it is a phantom, and parametricity (Part 7) guarantees that tag-generic code cannot distinguish or convert between tags.

### Typestate: ServerSocket<Unbound> to Listening

The server socket lifecycle in `include/loom/http/server.hpp` is a typestate protocol:

```cpp
struct Unbound {};
struct Bound {};
struct Listening {};

template<typename State> class ServerSocket;

template<>
class ServerSocket<Unbound>
{
    FileDescriptor fd_;
public:
    explicit ServerSocket(FileDescriptor fd) : fd_(std::move(fd)) {}
    [[nodiscard]] auto bind(int port) && -> ServerSocket<Bound>;
};

template<>
class ServerSocket<Bound>
{
    FileDescriptor fd_;
public:
    explicit ServerSocket(FileDescriptor fd) : fd_(std::move(fd)) {}
    [[nodiscard]] auto listen() && -> ServerSocket<Listening>;
};

template<>
class ServerSocket<Listening>
{
    FileDescriptor fd_;
public:
    explicit ServerSocket(FileDescriptor fd) : fd_(std::move(fd)) {}
    int fd() const noexcept { return fd_.get(); }
};
```

Three states, three types. The transitions are `&&`-qualified methods — they consume `*this` by move, producing the next state type. The `HttpServer` constructor requires a `ServerSocket<Listening>`:

```cpp
explicit HttpServer(ServerSocket<Listening> socket);
```

You cannot construct an `HttpServer` with an unbound or merely bound socket — it is a type error. The protocol is:

```cpp
auto socket = create_server_socket()   // ServerSocket<Unbound>
    .bind(8080)                        // ServerSocket<Bound>
    .listen();                         // ServerSocket<Listening>

HttpServer server(std::move(socket));  // only Listening accepted
```

Calling `.listen()` on an `Unbound` socket is a compile error. Calling `.bind()` on a `Listening` socket is a compile error. The state machine is enforced by the type system.

### Concepts: ContentSource, WatchPolicy as Logical Propositions

Loom defines concepts as propositions that types must satisfy:

```cpp
// include/loom/content/content_source.hpp
template<typename T>
concept ContentSource =
requires(T source)
{
    { source.all_posts() } -> std::same_as<std::vector<Post>>;
    { source.all_pages() } -> std::same_as<std::vector<Page>>;
};

// include/loom/reload/watch_policy.hpp
template<typename W>
concept WatchPolicy = requires(W w)
{
    { w.poll() } -> std::same_as<std::optional<ChangeSet>>;
    { w.start() } -> std::same_as<void>;
    { w.stop()  } -> std::same_as<void>;
};

template<typename S>
concept Reloadable = requires(S s, const ChangeSet& cs)
{
    { s.reload(cs) } -> std::same_as<void>;
};
```

Each concept is a proposition in intuitionistic logic (Part 5). `ContentSource<T>` is the proposition "T provides `all_posts()` returning `vector<Post>` and `all_pages()` returning `vector<Page>`." A type that satisfies the concept is a proof of the proposition — a constructive witness.

The `HotReloader` uses concepts as logical conjunctions — the `requires` clause demands that `Source` simultaneously satisfies `ContentSource` AND `Reloadable`:

```cpp
template<typename Source, WatchPolicy Watcher, typename Cache>
    requires ContentSource<Source> && Reloadable<Source>
class HotReloader { /* ... */ };
```

This is conjunction introduction (Part 5): the conjunction `ContentSource<Source> ∧ Reloadable<Source>` must be proven by the instantiating type. If a source type provides posts and pages but cannot reload, the template will not instantiate — the proof is incomplete.

### Compile-Time Data: Route Table with NTTPs

Loom's routing DSL (`include/loom/http/route.hpp`) uses compile-time strings as non-type template parameters:

```cpp
template<Lit P> inline constexpr get_t<P>  get{};

auto dispatch = compile(
    fallback(not_found_handler),
    get<"/">(index_handler),
    get<"/post/:slug">(post_handler),
    get<"/tag/:slug">(tag_handler)
);
```

Each route is a `Route<HttpMethod::GET, Lit<"/post/:slug">, H>` — a type parameterized by a compile-time string value. The `Traits<P>` struct analyzes the pattern at compile time with `consteval` methods:

```cpp
template<Lit P>
struct Traits {
    static consteval bool is_static() noexcept
    {
        for (size_t i = 0; i < P.size(); ++i)
            if (P[i] == ':') return false;
        return true;
    }
};
```

The dispatch function is assembled by a fold expression over the route tuple — the compiler sees every pattern as a constant and generates an optimal if-else chain. No vtable, no hash map, no heap. This is Pi types in practice: the dispatch behavior depends on the route pattern value.

### Parametricity: AtomicCache<T> and HotReloader Generics

`AtomicCache<T>` is parametric in `T`:

```cpp
template<typename T>
class AtomicCache
{
    std::atomic<std::shared_ptr<const T>> data_;
public:
    explicit AtomicCache(std::shared_ptr<const T> initial)
        : data_(std::move(initial)) {}

    std::shared_ptr<const T> load() const noexcept
    {
        return data_.load(std::memory_order_acquire);
    }

    void store(std::shared_ptr<const T> next) noexcept
    {
        data_.store(std::move(next), std::memory_order_release);
    }
};
```

The cache never inspects `T`. It atomically swaps `shared_ptr<const T>` pointers — that is all. The free theorem: the cache's behavior is identical regardless of what `T` is. It is a pure container, and parametricity guarantees it cannot corrupt or depend on the cached data.

The `HotReloader` is parametric in `Cache` but ad-hoc polymorphic in `Source` and `Watcher` (constrained by concepts). The `RebuildFn` takes a source and a change set and produces a new cache value — the reloader treats the result as an opaque pointer. This separation of concerns — ad-hoc polymorphism for the operations that require specific interfaces, parametric polymorphism for the data that should remain abstract — is the practical application of the parametric/ad-hoc distinction.

### Substructural Types: FileDescriptor as Affine Type

`FileDescriptor` (`include/loom/core/fd.hpp`) is Loom's foundational affine type:

```cpp
class FileDescriptor
{
public:
    explicit FileDescriptor(int fd) noexcept : fd_(fd) {}
    ~FileDescriptor() { if (fd_ >= 0) ::close(fd_); }

    FileDescriptor(const FileDescriptor&) = delete;            // no contraction
    FileDescriptor& operator=(const FileDescriptor&) = delete; // no contraction
    FileDescriptor(FileDescriptor&& other) noexcept            // ownership transfer
        : fd_(other.fd_) { other.fd_ = -1; }

    [[nodiscard]] int release() noexcept;
};
```

Deleted copy = no contraction (no double-close). Destructor = weakening with cleanup (auto-close on discard). Move = linear implication (`A ⊸ A`). The `[[nodiscard]]` on `release()` is a relevance hint — the caller should not ignore the raw fd.

Every `ServerSocket<State>` wraps a `FileDescriptor`. The socket typestate transitions move the inner fd from state to state — at no point does a second owner exist. The affine discipline propagates upward: because `FileDescriptor` is move-only, `ServerSocket` is move-only, and the entire server socket lifecycle is substructurally controlled.

### Recursive Types: DOM Node Tree

The DOM system (`include/loom/render/dom.hpp`) defines a recursive type:

```cpp
struct Node
{
    enum Kind { Element, Void, Text, Raw, Fragment } kind = Fragment;
    std::string tag;
    std::vector<Attr> attrs;
    std::vector<Node> children;   // recursive: Node contains Nodes
    std::string content;
};
```

`Node` contains `std::vector<Node>` — children that are themselves nodes. This is `mu X. F(X)` where `F(X) = Tag * Attrs * List<X> + ... + List<X>`. The fixed point ties the knot: a `Node` tree can be arbitrarily deep.

The `render_to()` method is a catamorphism that tears down the tree into an HTML string. The element factories (`elem`, `div`, `h1`, `article`, etc.) and the `each()` combinator are anamorphisms that build the tree from domain data. A typical page render is a hylomorphism: unfold posts into a `Node` tree, then immediately fold it into HTML.

### The Synthesis

These are not isolated techniques applied independently. They form an interconnected system:

1. The **server socket typestate** wraps an **affine FileDescriptor**, ensuring the fd is never duplicated and always cleaned up.

2. The **HttpServer** constructor gates on `ServerSocket<Listening>` — a **concept-like type constraint** ensuring the protocol was followed.

3. Incoming data is parsed into a **sum type** (`ParseResult = HttpRequest + ParseError`), forcing exhaustive handling.

4. The request is dispatched through a **compile-time route table** where patterns are NTTPs and the dispatch chain is a **fold expression** — Pi types and staged computation.

5. Route handlers receive requests with **strongly-typed** parameters (`Slug`, `Title`) protected by **phantom tags** and **parametricity**.

6. The response is a **sum type** (`DynamicResponse + PrebuiltResponse`). Prebuilt responses come from an **AtomicCache<T>** that is **parametric** in the cache type.

7. The cache is maintained by a **HotReloader** constrained by **concepts** (`ContentSource`, `WatchPolicy`, `Reloadable`) — logical propositions proved by the source and watcher types.

8. Response HTML is generated by a **recursive DOM type** (`Node` containing `Node`s) — a fixed point rendered by a **catamorphism**.

9. The connection lifecycle is an **algebraic sum type** (`ReadPhase + WritePhase`), with transitions that consume and replace the phase — **linear implications** within the variant.

10. Throughout, **affine discipline** prevents resource leaks: file descriptors auto-close, sockets cannot be duplicated, connections are owned by the event loop.

The type system is not a bureaucratic overhead imposed on a working system. It is the system's skeleton. Remove the types and you have a bag of functions with implicit contracts documented in comments. Keep the types and the compiler verifies the contracts — every protocol step, every resource lifecycle, every state transition, every exhaustive match — at zero runtime cost.

This is what type-theoretic C++ looks like in production: not an academic exercise, but a practical methodology where the compiler does the work that would otherwise require tests, assertions, documentation, and discipline. The theory from this series — algebraic types, phantom types, typestate, concepts, dependent types, parametricity, substructural logic, recursive types — is not abstract. It is the vocabulary for describing what Loom's code already does.

## The Journey

We started with a provocation: *a program is a theory*. Over ten posts, we built that into a practical methodology grounded in real type theory:

1. **Types are propositions.** Values are proofs. The compiler is a theorem prover. ([Part 1](/post/type-theoretic-foundations) — type judgments, Curry-Howard, formation/introduction/elimination rules)

2. **Types have algebra.** Products multiply, sums add, the distributive law lets you factor. Recursive types are fixed points. Folds are catamorphisms. ([Part 2](/post/algebraic-data-types), [Part 9](/post/recursive-types-and-fixed-points) — semirings, initial algebras, F-algebras)

3. **Phantoms encode invisible truths.** Zero-cost type distinctions, safe because parametricity prevents tag inspection. ([Part 3](/post/phantom-types) — representation independence, proof witnesses)

4. **State lives in types.** Transitions consume and produce. Linear logic ensures resources are tracked. ([Part 4](/post/typestate-programming) — linear implication, session types)

5. **Concepts are logic.** Natural deduction with introduction and elimination rules. Intuitionistic: constructive proofs only. ([Part 5](/post/concepts-as-logic) — BHK interpretation, modus ponens via subsumption)

6. **Values enter types.** Dependent typing through NTTPs. The compiler is a staged computation engine. ([Part 6](/post/compile-time-data) — Pi types, Sigma types, the lambda cube, multi-stage programming)

7. **Types constrain behavior.** Parametricity gives theorems from signatures alone. The less a function can do, the more you know. ([Part 7](/post/parametricity) — Reynolds, free theorems, naturality)

8. **Resources demand discipline.** Affine types prevent duplication. Linear types ensure consumption. RAII is substructural logic in practice. ([Part 8](/post/substructural-types) — Girard's linear logic, structural rules)

9. **Data is algebra.** Recursive types are fixed points. Every fold is a catamorphism. Every unfold is an anamorphism. ([Part 9](/post/recursive-types-and-fixed-points) — mu types, F-algebras, hylomorphisms)

## Closing: Does It Compile?

The compiler is not your enemy. It is not a gatekeeping bureaucrat that exists to reject your code. It is a collaborator — perfectly precise, infinitely patient, incapable of forgetting a rule. The more information you give it through your types, the more work it does for you.

The question is no longer "will this code work?" The question is: **does it compile?**

If it compiles, the protocol is followed. The states are consistent. The transitions are valid. The invariants hold. The resources are tracked. The phantoms are propagated. The proofs are checked. And the runtime is free to focus on the one thing that genuinely requires execution: interacting with the real world.

A program is a theory. The types are its axioms. The functions are its theorems. The compiler is its proof checker. And when the proof checks out — when the types align, the states match, the concepts are satisfied — you have not just written code that works. You have built a system that *cannot work any other way*.

That is the beauty of type-theoretic C++.
