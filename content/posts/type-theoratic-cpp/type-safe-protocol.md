---
title: "Building a Type-Safe Protocol — The Grand Synthesis"
date: 2026-04-07
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
