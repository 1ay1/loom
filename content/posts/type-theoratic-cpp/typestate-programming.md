---
title: "Typestate Programming — When Types Remember What Happened"
date: 2026-03-24
slug: typestate-programming
tags: [c++20, type-theory, typestate, state-machines, linear-logic, protocols]
excerpt: "States as types. Transitions as functions that consume and produce. Grounded in linear logic — where every resource must be used exactly once — typestate turns protocol violations into compile errors."
---

In [part 4](/post/phantom-types) we used phantom types to distinguish values that look the same but mean different things. Now we go further. Instead of tagging values with *what they are*, we tag them with *where they are in a process* — encoding sequential protocols directly in the type system.

The idea is called **typestate**: the state of an object is part of its type. Transitions between states are function calls that *consume* a value of one type and *produce* a value of another. If you try to skip a step or repeat a step or do steps out of order, the program does not compile.

The theoretical foundation is **linear logic** — a logic where every hypothesis must be used exactly once. This is the logic of resources, and it maps perfectly to state machines.

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
    // ... every method checks state_ ...
};
```

Every method starts with a state check. The checks are:

- **Invisible** — the state requirement is not in the function signature
- **Testable only by execution** — violations are found at runtime
- **Forgettable** — nothing stops you from adding a method that skips the check
- **Redundant** — `Created` has a `response_` field; `Closed` has a `socket_fd_`

The type `HttpConnection` admits all states at all times. It carries data meaningful only in some states and relies on the programmer to not touch it in others.

## Typestate: States as Types

```cpp
struct Created { std::string host; };
struct Connected { int socket_fd; };
struct RequestSent { int socket_fd; };
struct ResponseReceived { int socket_fd; std::string response; };
```

Each type carries exactly the data relevant to its state. No dead fields.

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

Protocol violations become type errors:

```cpp
auto c = Created{"example.com"};
send_request(std::move(c), "GET /");  // ERROR: Created is not Connected
```

```cpp
auto conn = connect(Created{"example.com"});
receive_response(std::move(conn));  // ERROR: Connected is not RequestSent
```

## Linear Logic: The Foundation

Typestate programming is grounded in **linear logic**, invented by Jean-Yves Girard in 1987. To understand why typestate works — and what its limits are — we need to understand what linear logic *is*.

In classical logic, you can use a premise as many times as you want. If you know "it is raining," you can use that fact in ten different deductions. You can also ignore premises — if you know "it is raining" but do not need that fact, you discard it.

These correspond to two **structural rules**:

- **Contraction**: use a premise multiple times (copying)
- **Weakening**: ignore a premise (discarding)

Linear logic removes both. In linear logic, every hypothesis must be used **exactly once**. You cannot copy it. You cannot discard it.

Why would you want this? Because *resources* work this way. A file handle must be closed exactly once — not zero times (leak), not twice (double-close). A database transaction must be committed or rolled back exactly once. A message must be consumed exactly once.

Linear logic has its own connectives, distinct from classical logic:

| Connective | Symbol | Meaning | C++ Analogue |
|---|---|---|---|
| Linear implication | A ⊸ B | Consuming an A produces a B | `auto f(A) -> B` (by move) |
| Tensor (multiplicative AND) | A ⊗ B | Have both, must use both | `pair<unique_ptr<A>, unique_ptr<B>>` |
| Plus (additive OR) | A ⊕ B | One or the other, must handle both | `variant<A, B>` (move-only) |
| Of course (exponential) | !A | Unlimited supply, copy/discard freely | `shared_ptr<A>`, copyable types |

The **linear implication** A ⊸ B (read "A lollipop B") is the key connective. It means: consuming one A produces one B. The A is *used up*. It does not persist after the implication fires.

Now look at typestate transitions:

```cpp
auto connect(Created c) -> Connected;      // Created ⊸ Connected
auto send(Connected c, Request r) -> Sent; // Connected ⊗ Request ⊸ Sent
auto recv(Sent s) -> Response;             // Sent ⊸ Response
```

Each transition is a linear implication. The input state is consumed. The output state is produced. The protocol is a chain of linear implications, and linear logic guarantees that every resource (every state handle) is accounted for.

## Affine vs Linear: Where C++ Stands

C++ does not have true linear types. It has something close: **affine types** through move semantics.

The substructural type hierarchy (explored fully in [part 9](/post/substructural-types)):

| System | Can copy? | Can discard? | Use pattern |
|---|---|---|---|
| **Unrestricted** | Yes | Yes | Any number of times |
| **Affine** | No | Yes | At most once |
| **Linear** | No | No | Exactly once |
| **Relevant** | Yes | No | At least once |

C++ move-only types (`unique_ptr`, `unique_lock`) are affine: you can move them (use once) or let them destruct (discard), but you cannot copy them. True linear types would require the compiler to reject code that constructs a value and never uses it — C++ does not enforce this (destructors always run, silently handling the "discard" case).

```cpp
auto conn = connect(Created{"example.com"});
auto sent = send_request(std::move(conn), "GET /");
// conn is now moved-from — affine semantics prevent reuse
// but if we never used conn at all, the destructor would silently run
```

The gap between affine and linear matters: a true linear type system would catch "forgotten" resources at compile time. C++ relies on RAII destructors to handle the discard case, which is pragmatically sound but theoretically weaker. Rust gets closer with its ownership system, where the borrow checker enforces affine rules and `Drop` handles cleanup.

## Branching Protocols

Real protocols branch. Authentication might succeed or fail:

```cpp
struct Unauthenticated { int socket_fd; };
struct Authenticated { int socket_fd; std::string token; };
struct AuthFailed { int socket_fd; std::string reason; };

auto authenticate(Unauthenticated conn, std::string credentials)
    -> std::variant<Authenticated, AuthFailed>
{
    if (success)
        return Authenticated{conn.socket_fd, token};
    else
        return AuthFailed{conn.socket_fd, reason};
}
```

The return type is a sum: `Authenticated ⊕ AuthFailed`. In linear logic, the additive disjunction requires the consumer to handle both cases:

```cpp
auto result = authenticate(std::move(conn), creds);

std::visit(overloaded{
    [](Authenticated auth) {
        auto data = fetch_data(std::move(auth));
    },
    [](AuthFailed fail) {
        log_error(fail.reason);
        close_unauthenticated(std::move(fail));
    },
}, result);
```

Each branch receives the correct state type with the correct data.

## The Builder Pattern, Type-Safe

Typestate gives compile-time verified builders:

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

`build()` only accepts `Ready`. Missing a step is a type error. With method chaining:

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

## Session Types: The Full Theory

Typestate is a practical fragment of **session types**, a theoretical framework where the type of a communication channel describes the *entire protocol*.

A session type specifies, from one participant's perspective, the sequence of sends and receives:

```
Client = !Request . ?Response . End
Server = ?Request . !Response . End
```

The `!` means "send," `?` means "receive," `.` means "then." The client sends a request, receives a response, then the session ends. The server is the **dual** — it receives what the client sends, sends what the client receives.

The key property: a well-typed program using session types *cannot* violate the protocol. If the client tries to receive before sending, the types do not match. If the server tries to send before receiving, same thing.

C++ typestate approximates session types for one side. Each state type corresponds to a point in the session. Each transition is a send or receive. The type system ensures operations happen in order. We lose automatic dual-party checking (the compiler does not verify that client and server types are duals), but we gain the core guarantee: protocol violations are type errors.

## The Continuation-Passing Connection

There is a deep connection between typestate and continuation-passing style (CPS). Each typestate transition takes the current state and produces the next state. The "continuation" is implicit — it is the set of operations available on the output type.

```cpp
auto connect(Created c) -> Connected;
// The "continuation" after connect is: any function that takes Connected
```

In CPS, you would write:

```cpp
template<typename K>
void connect_cps(Created c, K continuation) {
    Connected result = /* ... */;
    continuation(std::move(result));
}
```

The continuation `K` is constrained by the output type — it must accept a `Connected`. This connects to Curry-Howard: `A ⊸ B` as "consuming a proof of A, producing a proof of B." The continuation is the consumer of the proof.

## Resource Lifecycle Encoding

RAII handles acquire-release beautifully. But some resources have multi-phase lifecycles:

```cpp
struct Begun { /* db handle */ };
struct Committed {};
struct RolledBack {};

auto begin_transaction(Database& db) -> Begun;
auto execute(Begun& txn, std::string_view sql) -> Result; // borrows, doesn't consume
auto commit(Begun txn) -> Committed;     // consumes
auto rollback(Begun txn) -> RolledBack;  // consumes
```

`execute` borrows (`Begun&`), while `commit` and `rollback` consume (`Begun` by value). You can execute many queries but commit/rollback only once. After `commit(std::move(txn))`, the handle is gone.

In linear logic terms: `execute` is non-linear (it uses `&`, a shared reference — the linear logic `!` modality applied locally). `commit` and `rollback` are linear (they consume the resource).

## When Typestate Costs Too Much

Typestate is not free in code complexity. Each state is a separate type. Generic functions need templates or variants. Guidelines:

**Use typestate for critical protocols.** Authentication, resource lifecycle, builders for complex objects — where violations cause security bugs, leaks, or corruption.

**Use runtime state for UI and configuration.** A toggle between "expanded" and "collapsed" does not need typestate. The cost of a runtime branch is negligible.

**Use typestate at API boundaries.** Library APIs benefit most — typestate prevents misuse at compile time. Inside a module where you control all call sites, runtime state may be simpler.

The litmus test: *would a violation of this protocol be a serious bug?* If yes, typestate makes it unconstructible. If no, a runtime flag is fine.

## In Loom

Loom's server socket uses typestate to enforce a three-phase lifecycle at compile time: create, bind, listen. The implementation lives in `include/loom/http/server.hpp`.

### The Three States

Each state is a distinct type. Each carries exactly the data relevant to that phase:

```cpp
struct Unbound {};
struct Bound {};
struct Listening {};

template<typename State>
class ServerSocket;
```

The template is specialized for each state:

```cpp
template<>
class ServerSocket<Unbound> {
    FileDescriptor fd_;
public:
    explicit ServerSocket(FileDescriptor fd) : fd_(std::move(fd)) {}

    // Unbound ⊸ Bound: consume the unbound socket, produce a bound one
    [[nodiscard]] auto bind(int port) && -> ServerSocket<Bound>;
};

template<>
class ServerSocket<Bound> {
    FileDescriptor fd_;
public:
    explicit ServerSocket(FileDescriptor fd) : fd_(std::move(fd)) {}

    // Bound ⊸ Listening: consume the bound socket, produce a listening one
    [[nodiscard]] auto listen() && -> ServerSocket<Listening>;
};

template<>
class ServerSocket<Listening> {
    FileDescriptor fd_;
public:
    explicit ServerSocket(FileDescriptor fd) : fd_(std::move(fd)) {}
    int fd() const noexcept { return fd_.get(); }
};
```

### Transitions Consume the Source

The `&&` qualifier on `bind()` and `listen()` is the key. It means the method can only be called on an rvalue — the source state is consumed by the transition. After calling `.bind(8080)`, the `ServerSocket<Unbound>` no longer exists:

```cpp
auto socket = create_server_socket()   // ServerSocket<Unbound>
    .bind(8080)                        // ServerSocket<Bound>  (Unbound consumed)
    .listen();                         // ServerSocket<Listening> (Bound consumed)
```

This is a chain of linear implications: `Unbound ⊸ Bound ⊸ Listening`.

### Protocol Violations Are Type Errors

Calling `.listen()` on an unbound socket does not compile — `ServerSocket<Unbound>` has no `listen()` method:

```cpp
auto socket = create_server_socket();
socket.listen();  // ERROR: no member 'listen' in ServerSocket<Unbound>
```

Calling `.bind()` on an already-listening socket does not compile either:

```cpp
auto listening = create_server_socket().bind(8080).listen();
listening.bind(9090);  // ERROR: no member 'bind' in ServerSocket<Listening>
```

### The HttpServer Requires the Final State

The server constructor accepts only a `ServerSocket<Listening>` — typestate guarantees that setup is complete before the server can be constructed:

```cpp
explicit HttpServer(ServerSocket<Listening> socket);
```

Passing a `ServerSocket<Unbound>` or `ServerSocket<Bound>` is a type error. The `HttpServer` constructor is, through Curry-Howard, a proof consumer: it requires evidence (a `ServerSocket<Listening>`) that the protocol has been followed.

### Affine Ownership

The `FileDescriptor` inside each socket specialization is move-only (affine). The socket itself is move-only by composition — `FileDescriptor`'s deleted copy constructor propagates to `ServerSocket`. This means the file descriptor cannot be duplicated, and ownership transfers cleanly through each typestate transition. The `[[nodiscard]]` annotation ensures the caller cannot silently discard the new state.

## Closing: Protocols Are Types

The traditional view of a protocol is a flowchart on a whiteboard. Developers study it, internalize it, and try to implement it correctly. When they get it wrong, the bug shows up at runtime.

The typestate view: the protocol *is* the type structure. The types *are* the states. The functions *are* the transitions. The compiler *is* the protocol verifier. Linear logic *is* the theory that makes it sound.

In the [next post](/post/concepts-as-logic), we step back and look at concepts through a type-theoretic lens — as logical propositions about types, where requires-expressions are proofs in natural deduction and concept composition follows the rules of intuitionistic logic.
