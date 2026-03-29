---
title: "Building a Type-Safe Protocol — Putting It All Together"
date: 2026-05-10
slug: type-safe-protocol
tags: [c++20, type-theory, typestate, protocols, compile-time, capstone]
excerpt: "We combine every technique from this series — phantom types, typestate, concepts, compile-time data — to build a compile-time verified protocol where misordering, missing steps, and invalid transitions are type errors, not runtime surprises."
---

This is the final post in the series. We have built up a vocabulary: [algebraic types](/post/algebraic-data-types) to model domain data, [phantom types](/post/phantom-types) to distinguish semantically different values, [typestate](/post/typestate-programming) to encode state machines, [concepts](/post/concepts-as-logic) to express logical predicates, and [compile-time data](/post/compile-time-data) to push computation before runtime.

Now we put them all together. We will build a type-safe HTTP client where the compiler verifies the entire request lifecycle: you must connect before sending, send before receiving, provide required headers, and close when done. Violating any of these rules is not a runtime error — it is a type error that prevents compilation.

This is not a toy example. The patterns here are the same ones used in Loom's internal architecture, and they scale to real protocol implementations.

## The Protocol

Our HTTP client has this lifecycle:

```
Idle → Connected → HeadersSent → BodySent → ResponseReceived → Done
```

With constraints:
- You must set `Host` and `Content-Type` headers before sending
- You can only read the response after the full request is sent
- After reading the response, the connection must be closed
- Connections cannot be reused (HTTP/1.0 style, for simplicity)

In a traditional implementation, each of these constraints is a runtime check. In our implementation, each is a compile-time guarantee.

## Step 1: State Types (Typestate)

We define a type for each protocol state. Each type carries exactly the data relevant to that state — nothing more:

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

`Idle` has no socket — you have not connected yet. `Connected` has a socket but no header state. `HeadersSent` tracks which required headers have been set. `BodySent` has sent everything and is waiting. `ResponseReceived` has the response data. Each state is a faithful representation of what exists at that point in the protocol.

## Step 2: Phantom Tags for Header Requirements (Phantom Types)

We need the compiler to track which headers have been set. We use phantom types:

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

`Headers<NoHost, NoCT>` means no required headers are set. `Headers<HasHost, NoCT>` means Host is set but Content-Type is not. `Headers<HasHost, HasCT>` means both are set. These are four different types, and only the last one can proceed to sending the body.

## Step 3: Transition Functions (Typestate + Move Semantics)

Each protocol step is a function that consumes the current state and produces the next:

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

// Optional headers — don't change the phantom state
template<typename HS, typename CS>
auto set_header(Headers<HS, CS> h, std::string key, std::string value)
    -> Headers<HS, CS>
{
    h.entries.emplace_back(std::move(key), std::move(value));
    return h;
}
```

Notice how the phantom types flow through the transitions. `set_host` changes `NoHost` to `HasHost` but preserves `CTState` — whatever the Content-Type state was before, it remains the same. `set_content_type` changes `NoCT` to `HasCT` but preserves `HostState`. These are independent axes of the type, and each function affects only the axis it is responsible for.

Also notice: `set_host` only accepts `Headers<NoHost, _>`. You cannot set Host twice — the type changes after the first call, and the function does not match `Headers<HasHost, _>`. Double-setting is a compile error.

## Step 4: The Gate (Concepts)

Sending the body requires both headers to be set. We express this with a concept:

```cpp
template<typename T>
concept ReadyToSend = requires {
    requires std::same_as<T, Headers<HasHost, HasCT>>;
};

auto send_body(Headers<HasHost, HasCT> h, std::string_view method,
               std::string_view path, std::string_view body) -> BodySent
{
    // Serialize and send the HTTP request
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

The function signature *is* the constraint: it only accepts `Headers<HasHost, HasCT>`. If you pass `Headers<HasHost, NoCT>`, the template deduction fails. The error message will say something like "no matching function for call to `send_body`" — and the types in the error tell you exactly what is missing.

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

Each function consumes its input by value. After `receive(std::move(sent))`, the `BodySent` value is gone. You cannot receive twice. After `close(std::move(resp))`, the `ResponseReceived` value is gone. You cannot close twice. The protocol is linear.

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

Each line advances the protocol by one step. The types change at each step. The compiler verifies the entire sequence.

Now watch what happens when you violate the protocol:

```cpp
// Forgot to set Content-Type:
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
// Tried to set Host twice:
auto hdrs = connect(Idle{}, "example.com", 80);
hdrs = set_host(std::move(hdrs), "example.com");
hdrs = set_host(std::move(hdrs), "other.com");
// ERROR: set_host() takes Headers<NoHost, _>, but hdrs is Headers<HasHost, _>
```

Every violation is a type error. The error messages are not cryptic template vomit — they name the specific state types involved, making the problem clear.

## Making It Ergonomic: The Fluent Interface

Free functions work but are verbose. We can wrap the protocol in a fluent builder:

```cpp
template<typename State>
class HttpClient {
    State state_;
public:
    explicit HttpClient(State s) : state_(std::move(s)) {}

    auto set_host(std::string_view host) -> HttpClient<Headers<HasHost, /* CT preserved */>>
        requires /* State is Headers<NoHost, _> */;

    auto set_content_type(std::string_view ct) -> HttpClient<Headers</* Host preserved */, HasCT>>
        requires /* State is Headers<_, NoCT> */;

    auto send(std::string_view method, std::string_view path, std::string_view body)
        -> HttpClient<BodySent>
        requires std::same_as<State, Headers<HasHost, HasCT>>;

    auto receive() -> HttpClient<ResponseReceived>
        requires std::same_as<State, BodySent>;

    auto response() const -> const ResponseReceived&
        requires std::same_as<State, ResponseReceived>;
};
```

Usage becomes:

```cpp
auto resp = HttpClient(connect(Idle{}, "example.com", 80))
    .set_host("example.com")
    .set_content_type("application/json")
    .send("POST", "/api", R"({"key": "value"})")
    .receive()
    .response();
```

Each method returns a new `HttpClient` with a different state parameter. The chain reads like prose: connect, set headers, send, receive. And any misordering is caught at the exact point of misuse.

## Adding Compile-Time Route Definitions

We can go further and define the API schema at compile time using the techniques from [part 6](/post/compile-time-data):

```cpp
template<Lit Method, Lit Path>
struct Endpoint {
    static constexpr auto method = Method.sv();
    static constexpr auto path = Path.sv();
};

using GetUsers = Endpoint<"GET", "/api/users">;
using CreateUser = Endpoint<"POST", "/api/users">;
using GetUser = Endpoint<"GET", "/api/users/:id">;

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

Now you define endpoints once and the compiler ensures you only send a body with POST/PUT/PATCH. A `request<GetUsers>(headers, "some body")` would fail to compile because GET endpoints do not have a body concept.

## The Cost

What does all of this cost at runtime? Nothing. Every phantom type is an empty struct. Every state type compiles to its data fields only. The template instantiation produces the same machine code as a hand-written version without types. The `std::move` calls compile away. The concept checks are compile-time only.

At `-O2`, the entire protocol sequence — connect, set headers, serialize request, send, receive, close — compiles to the same instructions as a hand-written C implementation that does the same steps in order. The type machinery is a compile-time scaffold that vanishes in the binary.

## Comparison: What We Gained

| Property | Runtime Checks | Type-Safe Protocol |
|---|---|---|
| Missing required header | Runtime exception | Compile error |
| Wrong method order | Runtime exception | Compile error |
| Double close | Runtime guard clause | Type error (moved-from) |
| Receive before send | Runtime assertion | Type error |
| Error discoverability | Requires test coverage | Immediate at compile time |
| Runtime cost | Check on every call | Zero |
| Documentation | External (comments, docs) | The types *are* the docs |

The type-safe version catches every protocol violation at compile time, has zero runtime overhead, and is self-documenting — the function signatures tell you exactly what state is required and what state is produced.

## The Design Recipe

We can now formalize the approach used throughout this series into a recipe for type-safe protocol design:

1. **Enumerate the states.** Draw the state machine. Each state becomes a type. Each type carries the data relevant to that state and nothing more.

2. **Define transitions as functions.** Each transition consumes the source state (by value/move) and produces the target state. The function signature *is* the transition specification.

3. **Use phantom types for independent axes.** If there are multiple independent requirements (like "has Host" and "has Content-Type"), use phantom parameters so each axis can be set independently.

4. **Gate critical transitions with concepts.** The entry to dangerous or irreversible operations (sending data, committing a transaction) should require a concept that encodes all prerequisites.

5. **Use compile-time data for static structure.** Route patterns, endpoint definitions, configuration — anything known at build time should be a template parameter, not a runtime value.

6. **Move to enforce linearity.** Pass state by value and use `std::move` so that each state value is used exactly once. This prevents aliasing and ensures the protocol advances monotonically.

## Beyond HTTP: Where This Applies

The patterns generalize to any sequential protocol:

- **Database transactions**: Begin → Execute* → Commit/Rollback
- **File I/O**: Open → Read/Write* → Close
- **Authentication flows**: Unauthenticated → Challenged → Authenticated/Denied
- **Build systems**: Configure → Compile → Link → Package
- **Payment processing**: Cart → Checkout → Payment → Confirmed/Failed
- **TLS handshake**: ClientHello → ServerHello → KeyExchange → Established

Any protocol with ordered steps and preconditions can be encoded this way. The types are the states. The functions are the transitions. The compiler is the verifier.

## Closing: The World the Compiler Sees

We started this series with a provocation: *a program is a theory*. Over six posts, we built that idea into a practical methodology:

- Types are not just data containers — they are **propositions** about the domain ([part 1](/post/type-theoretic-foundations))
- Products and sums give types an **algebra** that matches the structure of the domain ([part 2](/post/algebraic-data-types))
- Phantom types create **zero-cost distinctions** the compiler can enforce ([part 3](/post/phantom-types))
- Typestate turns state machines into **type-level protocols** where violations are compile errors ([part 4](/post/typestate-programming))
- Concepts are **logical predicates** that compose according to propositional logic ([part 5](/post/concepts-as-logic))
- Compile-time data blurs the boundary between types and values, letting the compiler **compute** before the program runs ([part 6](/post/compile-time-data))
- And in this post, we combined them all into a **self-verifying protocol** with zero runtime cost

The compiler is not your enemy. It is not a gatekeeping bureaucrat that exists to reject your code. It is a collaborator — one that is perfectly precise, infinitely patient, and incapable of forgetting a rule. The more information you give it through your types, the more work it can do for you.

The question is no longer "will this code work?" The question is: **does it compile?**

If it compiles, the protocol is followed. The states are consistent. The transitions are valid. The invariants hold. And the runtime is free to focus on the one thing that genuinely requires execution: interacting with the real world.

That is the beauty of type-theoretic C++.
