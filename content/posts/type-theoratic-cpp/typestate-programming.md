---
title: "Typestate Programming — When Types Remember What Happened"
date: 2026-03-29
slug: typestate-programming
tags: [c++20, type-theory, typestate, state-machines, protocols]
excerpt: "A typestate system encodes a state machine in the type system. States are types. Transitions are functions that consume one type and produce another. The compiler enforces the protocol — calling things in the wrong order is not a runtime error, it is a type error."
---

In [part 3](/post/phantom-types) we used phantom types to distinguish values that look the same but mean different things. Now we go further. Instead of tagging values with *what they are*, we tag them with *where they are in a process* — encoding sequential protocols directly in the type system.

The idea is called **typestate**: the state of an object is part of its type. Transitions between states are function calls that *consume* a value of one type and *produce* a value of another. If you try to skip a step or repeat a step or do steps out of order, the program does not compile.

This is not a runtime check. It is a structural impossibility.

## The Problem: State Machines at Runtime

Most stateful protocols are encoded with a runtime flag:

```cpp
class HttpConnection {
    enum class State { Created, Connected, RequestSent, ResponseReceived, Closed };
    State state_ = State::Created;
    int socket_fd_ = -1;
    std::string response_;
public:
    void connect(std::string host) {
        if (state_ != State::Created) throw std::logic_error("already connected");
        socket_fd_ = /* ... */;
        state_ = State::Connected;
    }
    void send_request(std::string req) {
        if (state_ != State::Connected) throw std::logic_error("not connected");
        /* ... */
        state_ = State::RequestSent;
    }
    auto receive_response() -> std::string {
        if (state_ != State::RequestSent) throw std::logic_error("no request sent");
        /* ... */
        state_ = State::ResponseReceived;
        return response_;
    }
    void close() {
        if (state_ == State::Closed) return;
        ::close(socket_fd_);
        state_ = State::Closed;
    }
};
```

This works, but notice the pattern: every method starts with a state check. The checks are runtime, which means they are:

- **Invisible** — calling code does not see the state requirement in the function signature
- **Testable only by execution** — you discover violations by running the code, not by compiling it
- **Forgettable** — nothing stops you from adding a new method that forgets to check `state_`
- **Redundant** — the same states are checked in multiple places with copy-pasted logic

The deeper issue: the type `HttpConnection` admits all states at all times. A `Created` connection has a `response_` field. A `Closed` connection has a `socket_fd_`. The struct carries data that is meaningless in most of its states, and relies on the programmer to not touch it.

## Typestate: States as Types

The typestate approach splits the class into one type per state:

```cpp
struct Created {
    std::string host;
};

struct Connected {
    int socket_fd;
};

struct RequestSent {
    int socket_fd;
};

struct ResponseReceived {
    int socket_fd;
    std::string response;
};
```

Each type carries exactly the data relevant to its state. `Created` has a host but no socket. `Connected` has a socket but no response. `ResponseReceived` has both. There are no dead fields, no meaningless combinations, no states where `socket_fd` is `-1` but the connection claims to be open.

Transitions are functions that consume one state and produce the next:

```cpp
auto connect(Created c) -> Connected {
    int fd = ::socket(/* ... */);
    // ... connect to c.host ...
    return Connected{fd};
}

auto send_request(Connected c, std::string request) -> RequestSent {
    ::write(c.socket_fd, request.data(), request.size());
    return RequestSent{c.socket_fd};
}

auto receive_response(RequestSent s) -> ResponseReceived {
    std::string response = /* read from s.socket_fd */;
    return ResponseReceived{s.socket_fd, std::move(response)};
}

auto close(ResponseReceived r) -> void {
    ::close(r.socket_fd);
}
```

The protocol is now a chain of types:

```
Created → Connected → RequestSent → ResponseReceived → (closed)
```

And using it:

```cpp
auto c = Created{"example.com"};
auto conn = connect(std::move(c));
auto sent = send_request(std::move(conn), "GET / HTTP/1.1\r\n\r\n");
auto resp = receive_response(std::move(sent));
close(std::move(resp));
```

Now try to violate the protocol:

```cpp
auto c = Created{"example.com"};
auto sent = send_request(std::move(c), "GET /");  // ERROR: Created is not Connected
```

This does not compile. `send_request` takes a `Connected`. You gave it a `Created`. The error message tells you exactly what went wrong: you tried to send a request before connecting.

```cpp
auto conn = connect(Created{"example.com"});
auto resp = receive_response(std::move(conn));  // ERROR: Connected is not RequestSent
```

Again, does not compile. You tried to receive a response before sending a request. The type system knows you skipped a step.

## Move Semantics as Linearity

There is a subtle but critical detail in the examples above: `std::move`. Every transition *consumes* its input. After `connect(std::move(c))`, the variable `c` is in a moved-from state — you cannot use it again (or rather, you can, but it is empty and useless).

This is an approximation of **linear types** — types that must be used exactly once. In a language with true linear types (like Rust's ownership system), the compiler *enforces* that a value is used once. In C++, move semantics get us most of the way there. After moving, the original variable is semantically dead. A good static analyzer or compiler warning (`-Wuse-after-move`) will flag reuse.

Why does linearity matter for typestate? Because it prevents *aliasing*. If you could hold onto a `Connected` value after passing it to `send_request`, you would have two views of the same connection in different states. The typestate invariant would be broken — is the connection `Connected` or `RequestSent`? Consuming the input (via move) ensures there is always exactly one handle, in exactly one state.

```cpp
auto conn = connect(Created{"example.com"});
auto sent = send_request(std::move(conn), "GET /");
// conn is now moved-from — using it here would be a bug
// sent is the only handle to the connection
```

## Branching Protocols

Real protocols branch. An authentication step might succeed or fail:

```cpp
struct Unauthenticated { int socket_fd; };
struct Authenticated { int socket_fd; std::string token; };
struct AuthFailed { int socket_fd; std::string reason; };

auto authenticate(Unauthenticated conn, std::string credentials)
    -> std::variant<Authenticated, AuthFailed>
{
    // ... attempt authentication ...
    if (success)
        return Authenticated{conn.socket_fd, token};
    else
        return AuthFailed{conn.socket_fd, reason};
}
```

The return type is a sum: `Authenticated + AuthFailed`. The caller must handle both cases:

```cpp
auto result = authenticate(std::move(conn), creds);

std::visit(overloaded{
    [](Authenticated auth) {
        auto data = fetch_data(std::move(auth));
        // ... use data ...
    },
    [](AuthFailed fail) {
        log_error(fail.reason);
        close_unauthenticated(std::move(fail));
    },
}, result);
```

The branching is explicit in the type. You cannot ignore the failure case — `std::visit` requires exhaustive handling. And each branch receives the correct state type, with the correct data, enabling only the operations that make sense for that branch.

## The Builder Pattern, Type-Safe

Typestate gives us a compile-time verified builder pattern. Instead of a builder that might forget a required field:

```cpp
// Traditional builder — can forget .host() and get a runtime error
auto req = RequestBuilder{}
    .method("GET")
    .path("/api/users")
    // forgot .host() — compiles fine, fails at runtime
    .build();
```

We use types to track which fields have been set:

```cpp
struct NeedsMethod {};
struct NeedsHost { std::string method; };
struct NeedsPath { std::string method; std::string host; };
struct Ready { std::string method; std::string host; std::string path; };

auto method(NeedsMethod, std::string m) -> NeedsHost {
    return {std::move(m)};
}
auto host(NeedsHost h, std::string ho) -> NeedsPath {
    return {std::move(h.method), std::move(ho)};
}
auto path(NeedsPath p, std::string pa) -> Ready {
    return {std::move(p.method), std::move(p.host), std::move(pa)};
}
auto build(Ready r) -> Request {
    return Request{std::move(r.method), std::move(r.host), std::move(r.path)};
}
```

Now `build()` only accepts `Ready`. You cannot call it before all fields are set because the type is wrong. The compiler tells you exactly which step you missed. No runtime validation needed.

With method chaining, this looks like:

```cpp
template<typename State>
struct Builder {
    State state;

    auto with_method(std::string m) -> Builder<NeedsHost>
        requires std::same_as<State, NeedsMethod>
    { return {{std::move(m)}}; }

    auto with_host(std::string h) -> Builder<NeedsPath>
        requires std::same_as<State, NeedsHost>
    { return {{std::move(state.method), std::move(h)}}; }

    auto with_path(std::string p) -> Builder<Ready>
        requires std::same_as<State, NeedsPath>
    { return {{std::move(state.method), std::move(state.host), std::move(p)}}; }

    auto build() -> Request
        requires std::same_as<State, Ready>
    { return {std::move(state.method), std::move(state.host), std::move(state.path)}; }
};
```

The `requires` clauses from [post #10 on concepts](/post/concepts-and-constraints) gate each method to the correct state. `with_host` only appears on `Builder<NeedsMethod>` after `with_method` has been called. The fluent interface compiles to the same code as direct struct construction — the state tracking vanishes.

## Resource Lifecycle Encoding

RAII handles the basic acquire-release pattern beautifully. But some resources have multi-phase lifecycles that RAII alone cannot express:

```cpp
// A database transaction: begin → (read/write)* → commit/rollback
struct Begun { /* db handle */ };
struct Committed {};
struct RolledBack {};

auto begin_transaction(Database& db) -> Begun;
auto execute(Begun& txn, std::string_view sql) -> Result;  // borrows, doesn't consume
auto commit(Begun txn) -> Committed;    // consumes — can't use txn after this
auto rollback(Begun txn) -> RolledBack; // consumes — can't use txn after this
```

Notice that `execute` takes `Begun&` (a reference — it borrows the transaction) while `commit` and `rollback` take `Begun` by value (they consume it). This means you can execute multiple queries on a live transaction, but committing or rolling back ends it. After `commit(std::move(txn))`, the transaction handle is gone. You cannot accidentally commit twice, or commit then rollback, or execute after committing.

## When Typestate Costs Too Much

Typestate is not free in terms of code complexity. Each state is a separate type. Functions that accept "any state" need templates or variants. Generic code over the protocol requires type erasure.

Pragmatic guidelines:

**Use typestate for critical protocols.** Authentication flows, resource lifecycle, builder patterns for complex objects — places where violating the order causes security bugs, resource leaks, or data corruption.

**Use runtime state for UI and configuration.** A UI component that toggles between "expanded" and "collapsed" does not need typestate. The cost of a runtime branch is negligible, and the state changes frequently in response to user actions.

**Use typestate at API boundaries.** If you are designing a library that other people will use, typestate prevents misuse at compile time. Inside a module where you control all call sites, runtime state may be simpler.

The litmus test: *would a violation of this protocol be a serious bug?* If yes, typestate makes it un-representable. If no, a runtime flag is fine.

## The Theory: Session Types

Typestate programming is a practical fragment of a theoretical idea called **session types**. In session type theory, the type of a communication channel describes the *entire protocol* — which messages can be sent and received, in which order, by which party. A well-typed program is guaranteed to follow the protocol.

C++ does not have built-in session types. But the typestate pattern we have been building approximates them: each state type corresponds to a point in the protocol, each transition function corresponds to a message, and the type system ensures messages happen in order. We lose some expressiveness (no first-class channel typing, no automatic dual-party checking) but we gain the core guarantee: **protocol violations are type errors**.

## Closing: Protocols Are Types

The traditional view of a protocol is a flowchart on a whiteboard. Developers study it, internalize it, and try to implement it correctly. When they get it wrong, the bug shows up at runtime — in tests if they are lucky, in production if they are not.

The typestate view is different. The protocol *is* the type structure. It is not documentation that exists separately from the code. It is the code. The types *are* the states. The functions *are* the transitions. And the compiler *is* the protocol verifier.

In the [next post](/post/concepts-as-logic), we step back from concrete patterns and look at concepts through a type-theoretic lens — as logical propositions about types, where requires-expressions are proofs and concept composition follows the rules of logic.
