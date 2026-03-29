---
title: "A Program Is a Theory — Foundations of Type-Theoretic C++"
date: 2026-03-29
slug: type-theoretic-foundations
tags: [c++20, type-theory, compile-time, design, foundations]
excerpt: "Reframing C++ as a language of types and proofs, where compile time enforces the structure of the world and invalid states become unrepresentable."
---

There is a quiet assumption behind most C++ code: a program is something that *runs*. You write it, compile it, execute it, and evaluate its behavior. The interesting part — the part that matters — happens at runtime.

That assumption is wrong. Or at least dangerously incomplete.

A program does run. But before it runs, it is *checked*. And that checking phase is not superficial syntax verification. It is structural. The compiler determines what kinds of entities may exist, how they may be constructed, which transformations are permitted, and which combinations are forbidden. This is not execution. This is **verification**. The program has not done anything yet, and already an enormous class of nonsense has been eliminated.

So a more accurate framing:

> A C++ program is a formal system whose internal consistency is verified before it is ever allowed to execute.

Or, more provocatively:

> A program is a *theory* of the world. And the compiler is its first reviewer.

This series is about taking that idea seriously. Not as a metaphor, but as a design methodology. We will use the C++ type system — templates, concepts, `constexpr`, phantom types, algebraic data types — to build programs where *incorrect states cannot be represented*, invalid transitions cannot be compiled, and the runtime is left with only the work that genuinely depends on reality.

If you have been following the [C++ series](/series/cpp), you have already seen the building blocks: [templates](/post/templates-and-generic-programming), [concepts](/post/concepts-and-constraints), [constexpr](/post/constexpr-consteval-and-compile-time), [fold expressions](/post/variadic-templates-and-fold-expressions), [variants](/post/variant-visit-and-sum-types). This series puts those tools to work inside a coherent theoretical framework.

## The Two Languages Inside C++

C++ is not one language. It is two, running at different times, speaking different dialects, but sharing the same syntax.

**The type language** runs at compile time. It describes what exists, what is valid, what is distinguishable, and what is impossible. Its vocabulary includes types, templates, concepts, constant expressions, and type relationships. It is declarative: you state what must be true, and the compiler either confirms it or rejects the program.

**The value language** runs at runtime. It describes what happens — how values flow, how effects occur, how the program interacts with external reality. It is imperative: you describe steps, and the CPU executes them.

Most codebases over-invest in the value language and under-invest in the type language. They write elaborate runtime validation — null checks, range checks, state flags, assertion macros — because their types are too weak to rule anything out. Every function defends itself against the universe because the type system has made no promises about what can arrive.

Type-theoretic C++ flips that balance. We push as much reasoning as possible into compile time, so that by the time a function runs, the only things that reach it are things that *should* reach it. The goal is not to eliminate runtime code. It is to ensure that runtime code only deals with genuine uncertainty — user input, network data, filesystem state — rather than with invariants that could have been guaranteed statically.

## Types as Propositions

The standard mental model is: a type is a set of values. `int` is the set of 32-bit integers. `std::string` is the set of character sequences. A variable of type `T` holds a value drawn from that set.

This is correct, but incomplete. There is a deeper interpretation from type theory, called the **Curry-Howard correspondence**:

> A type is a *proposition*. A value of that type is a *proof* that the proposition holds.

We do not get this perfectly in C++ — it is not Agda or Coq — but we get enough of it to reshape how we design systems. Consider:

```cpp
struct NonEmptyString {
private:
    std::string value_;
public:
    static auto from(std::string s) -> std::optional<NonEmptyString> {
        if (s.empty()) return std::nullopt;
        return NonEmptyString{std::move(s)};
    }
    auto get() const -> const std::string& { return value_; }
private:
    explicit NonEmptyString(std::string s) : value_(std::move(s)) {}
};
```

This is not just "a string wrapper." It is a *claim*: this string is non-empty. The private constructor means you cannot fabricate the claim — you must go through `from()`, which checks the evidence. If `from()` returns a value, that value is a *witness* to the property. Holding a `NonEmptyString` is having proof of non-emptiness.

Now any function that takes `NonEmptyString` as a parameter is stating a precondition:

```cpp
auto first_char(NonEmptyString s) -> char {
    return s.get()[0];  // always safe — the type guarantees it
}
```

No check needed. No assertion. No "but what if it's empty?" The type rules that out at the call site. The proof was established at construction, and the type system carries it forward.

This is the key shift: **types do not just describe shape. They encode truths.** And the compiler propagates those truths through the entire program, for free.

## Illegal States Are Logical Contradictions

The popular advice "make invalid states unrepresentable" is usually presented as a pragmatic design tip. It is deeper than that. In type-theoretic terms:

> Making invalid states unrepresentable means making logical contradictions unconstructible.

Consider the classic bad design:

```cpp
struct File {
    int fd;
    bool open;
};
```

This permits four combinations. Two make sense (`fd = 42, open = true` and `fd = -1, open = false`). Two are contradictions (`fd = -1, open = true` — an open file with no descriptor; `fd = 42, open = false` — a closed file that holds a live descriptor).

The type permits contradictions, so every function that touches `File` must defend against them. You end up with checks scattered everywhere — in `read()`, in `write()`, in `close()`, in the destructor — each one a confession that the type system has not done its job.

Now:

```cpp
struct OpenFile {
    int fd;  // guaranteed valid
    auto read(std::span<char> buf) -> ssize_t;
    auto close() -> ClosedFile;
};

struct ClosedFile {
    // no fd — nothing to leak, nothing to misuse
};
```

The contradictions are gone. An `OpenFile` cannot be closed — it holds a valid descriptor, period. A `ClosedFile` cannot be open — it does not even have a descriptor field. We did not add validation. We removed the need for it by making the types faithful to the domain.

This is not just cleaner design. It is **logical refinement**. We narrowed the type until it only admits states that correspond to reality.

## Structural vs Semantic Identity

One of the most common type-theoretic sins in C++ is structural conflation:

```cpp
void transfer(int from_account, int to_account, int amount_cents);
```

Three `int`s. The compiler sees three identical types. But semantically, a source account is not a destination account, and neither is an amount. This function is trivially misusable:

```cpp
transfer(amount, to, from);  // compiles fine. sends money backwards.
```

The bug is not in the logic. It is in the *representation*. The type system was given no semantic information, so it cannot catch semantic errors.

```cpp
struct AccountId { int value; };
struct Amount { int cents; };

void transfer(AccountId from, AccountId to, Amount amount);
```

Now `transfer(amount, to, from)` does not compile. We moved meaning from the programmer's head — or worse, from a comment — into the type system. The compiler enforces what was previously a convention.

This idea scales. In the [strong types post](/post/strong-types), we saw Loom use this pattern for `Slug`, `Title`, `PostId`, `Tag`, `Content`, `Series` — all strings underneath, all distinct types at the surface. A function that expects a `Slug` cannot accidentally receive a `Title`. That is not a runtime check. It is a compile-time impossibility.

## Distributed vs Localized Correctness

Weak types create a pattern we might call **distributed correctness**: since no type makes any promises, every function must independently verify every assumption. Validation is not centralized anywhere — it is *hoped for everywhere*.

```cpp
void process_order(int order_id, int user_id, std::string email) {
    if (order_id <= 0) throw std::invalid_argument("bad order id");
    if (user_id <= 0) throw std::invalid_argument("bad user id");
    if (email.find('@') == std::string::npos) throw std::invalid_argument("bad email");
    // now do the actual work...
}
```

Every function in the system has some version of this preamble. The checks are redundant, inconsistent, and easy to forget. Worse, they run every time — not because the values *might* be bad, but because the types cannot promise they are good.

Strong types create **localized correctness**: validation happens once, at construction, and the type carries the guarantee forward.

```cpp
void process_order(OrderId order, UserId user, Email email) {
    // just do the work — the types are the proof
}
```

The validation still exists. It just happens at the boundary — where the `OrderId` was created from raw input, where the `Email` was parsed from a string. After that point, every function downstream can trust its inputs without re-checking. The types are load-bearing.

## Typestate: Encoding Time in Types

Most programs have state machines. A connection starts disconnected, becomes connected, then authenticated. A file is opened, written to, closed. A transaction is started, committed or rolled back.

The standard encoding uses a runtime flag:

```cpp
enum class ConnState { Disconnected, Connected, Authenticated };

struct Connection {
    ConnState state;
    int socket_fd;
    std::string auth_token;
};
```

Now every method must check the flag. `send()` must verify `state == Connected || state == Authenticated`. `authenticate()` must verify `state == Connected`. These checks are runtime, verbose, and forgettable.

**Typestate** encodes the state in the type itself:

```cpp
struct Disconnected {};
struct Connected { int socket_fd; };
struct Authenticated { int socket_fd; std::string token; };

auto connect(Disconnected, std::string host) -> Connected;
auto authenticate(Connected, std::string creds) -> Authenticated;
auto send(Authenticated&, std::span<const char> data) -> size_t;
```

Now the state machine is the type system. You cannot call `authenticate` on a `Disconnected` — the overload does not exist. You cannot call `send` on a `Connected` — the parameter type does not match. The state transitions are function signatures, and the compiler verifies the protocol.

Notice something else: `Disconnected` has no socket. `Connected` has a socket but no token. `Authenticated` has both. Each type carries exactly the data that is relevant to its state. There is no `socket_fd` sitting around in a disconnected value, tempting someone to use it.

State is no longer *checked*. It is *encoded*. We will build this idea into a full protocol enforcement system in [part 4](/post/typestate-programming) of this series.

## The Compiler as Constraint Solver

When templates, concepts, and `constexpr` interact, something interesting happens. The compiler stops being a translator (source code in, machine code out) and starts behaving like a **constraint solver**: you state what must be true, and it determines whether your program satisfies those constraints.

```cpp
template<typename T>
concept Addable = requires(T a, T b) {
    { a + b } -> std::convertible_to<T>;
};

template<typename T>
concept Printable = requires(T a, std::ostream& os) {
    { os << a };
};

template<typename T>
    requires Addable<T> && Printable<T>
auto sum_and_print(std::span<const T> values) -> T {
    T result{};
    for (const auto& v : values) result = result + v;
    std::cout << "Sum: " << result << '\n';
    return result;
}
```

This function does not say "T is an int" or "T is a double." It says: T must support addition that produces another T, and T must be printable. Those are *logical predicates*, and the compiler checks them at instantiation time. If you pass a type that does not satisfy both constraints, the error message tells you exactly which proposition failed.

This is the beginning of something powerful. Concepts are not just "better SFINAE" or "nicer error messages." They are the closest thing C++ has to a logic of propositions about types. We will explore this deeply in [part 5](/post/concepts-as-logic) of this series.

## Compile Time vs Runtime: A Phase Distinction

A common mistake is treating compile time as an optimization — a way to make things faster. It is not. It is a *different phase of reasoning* with fundamentally different properties.

| Compile Time | Runtime |
|---|---|
| What is *allowed* | What *happens* |
| Structure | Behavior |
| Proof | Execution |
| Deterministic | Contingent |
| Universal | Situational |

A compile-time error is deterministic: it happens on every build, on every machine, regardless of input. A runtime error is situational: it depends on data, timing, environment. A compile-time check scales to every possible execution; a runtime check applies only to the execution that triggered it.

The practical consequence: if a property can be verified at compile time, it *should* be. Not for performance (though that helps), but because a bug caught at compile time is a bug that **never existed**. No user ever encountered it. No log ever recorded it. No test ever needed to cover it. It was ruled out before the program was born.

This is why `constexpr` and `consteval` matter beyond "fast math." They move computation from the contingent world of runtime into the deterministic world of compilation. A `constexpr` function is a function that the compiler can evaluate as a *theorem prover* — given constant inputs, it produces a constant result, or it fails to compile. There is no "sometimes works."

## Zero-Cost Abstraction: Why This Isn't Academic

Everything described so far would be interesting but impractical if it carried runtime cost. Languages with rich type systems — Haskell, Rust, Scala — sometimes impose overhead for their type machinery (boxing, vtables, runtime tags).

C++ does not. A `struct UserId { int value; }` compiles to the same machine code as a bare `int`. A phantom type parameter adds zero bytes to the struct layout. A concept-constrained template instantiates to the same code as an unconstrained one. Typestate markers are empty structs that exist only in the type system and vanish in the binary.

This is the unique position of C++: it has enough type-system expressiveness to encode meaningful invariants, and enough low-level control to ensure that encoding costs nothing at runtime. You get stronger guarantees, clearer intent, and identical performance. Among mainstream systems languages, this combination is unmatched.

## The Design Discipline

We arrive at the core methodology. When facing a new design problem, instead of asking "how do I implement this?", we ask a sequence of type-theoretic questions:

**What exists?** What are the fundamental entities in this domain? Not the operations — the *things*. Users, connections, tokens, documents, routes.

**What distinctions matter?** Which differences must the compiler understand? A user ID is not an order ID. A connected socket is not a listening socket. An unvalidated email is not a validated one.

**What is impossible?** What states must never be representable? An authenticated connection without a token. A negative price. An empty collection passed to a function that requires at least one element.

**What are the valid transitions?** How does one valid state become another? A disconnected connection becomes connected. A connected connection becomes authenticated. The types of these transitions are function signatures.

**What can be decided at compile time?** What depends only on the program's structure, not on runtime data? Route matching, configuration schemas, protocol shapes, unit conversions.

**What remains for runtime?** Only what genuinely depends on the external world: user input, network responses, file contents, wall-clock time.

This order matters. We start with the world (what exists), move to the constraints (what is valid), then to the dynamics (what changes), and only finally to execution (what happens). Most developers start at the last step. The type-theoretic approach starts at the first.

## A Taste of What's Coming

Let's combine several ideas into a single, small example:

```cpp
// Boundary types — raw vs validated
struct RawEmail { std::string value; };

struct Email {
private:
    std::string value_;
    explicit Email(std::string s) : value_(std::move(s)) {}
    friend auto validate(RawEmail) -> std::optional<Email>;
public:
    auto get() const -> const std::string& { return value_; }
};

auto validate(RawEmail raw) -> std::optional<Email> {
    auto at = raw.value.find('@');
    if (at == std::string::npos || at == 0 || at == raw.value.size() - 1)
        return std::nullopt;
    return Email{std::move(raw.value)};
}

// Downstream code never checks again
void send_welcome(Email to);
void add_to_newsletter(Email subscriber);
void notify_admin(Email admin, std::string_view event);
```

Raw input enters as `RawEmail`. The `validate` function is the only gate to `Email`. If it returns a value, that value is proof of validity. Every function downstream accepts `Email` and never needs to check. The validation exists *once*, at the boundary. The guarantee propagates everywhere, at zero cost.

This pattern — *parse, don't validate* — is one of the central techniques of type-theoretic design, and it generalizes far beyond email addresses.

## The Road Ahead

This is the first post in a series. We are not doing theory for its own sake. We are building toward real systems where protocols are encoded in types, state machines are enforced structurally, APIs are impossible to misuse, and all of it compiles down to the same machine code as the hand-written alternative.

The series will cover:

* **[Sum Types and Product Types](/post/algebraic-data-types)** — the algebra of C++ types, `std::variant` as tagged union, `std::tuple` as anonymous product, and why every domain model is an algebraic expression
* **[Phantom Types](/post/phantom-types)** — zero-cost type distinctions, the newtype pattern, tag-based dispatch, and making the compiler see what is not there
* **[Typestate Programming](/post/typestate-programming)** — encoding state machines in the type system, consuming values to enforce transitions, and protocols that cannot be violated
* **[Concepts as Logic](/post/concepts-as-logic)** — concepts as predicates, requires-expressions as proofs, concept composition as logical connectives, and the Curry-Howard bridge
* **[Compile-Time Data](/post/compile-time-data)** — non-type template parameters as values, compile-time strings, literal types, and the blurring line between types and data
* **[Building a Type-Safe Protocol](/post/type-safe-protocol)** — putting it all together to build a real, compile-time verified protocol stack

Each post will include real C++ code, real design patterns, and connections to the type-theoretic ideas that motivate them.

## Closing Thought

Most bugs are not subtle. They come from mixing things that should not mix, calling functions at the wrong time, forgetting to check an invariant, or representing a domain too loosely. All of these are failures of *representation* — the types permitted something that the domain forbids.

The fix is not more defensive code. It is better types.

The goal is not "write better functions." The goal is **design a world where bad functions cannot exist.** Once that world is built — once the types faithfully represent the domain — everything else becomes simpler. Functions get shorter. Tests get fewer. Edge cases vanish. And the compiler, that strict, pedantic, slightly judgmental machine, becomes your most reliable collaborator.

Let's build that world.
