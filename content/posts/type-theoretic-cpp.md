---
title: Type-Theoretic Modern C++: Making Illegal States Unrepresentable
slug: type-theoretic-cpp
date: 2026-03-14
tags: c++, type-theory, modern-cpp, programming
excerpt: A deep dive into using C++20's type system as a proof engine — strong types, concepts, variants, and compile-time contracts that eliminate entire categories of bugs before your code ever runs.
---

Most C++ tutorials teach you the language. This one teaches you how to think *with* the type system. We'll treat types not as annotations the compiler needs, but as propositions about your program's correctness — propositions the compiler *proves* for you at build time.

This is type-theoretic programming. The idea comes from the Curry-Howard correspondence: types are propositions, programs are proofs. You don't need a PhD to use it. You need C++20 and the willingness to let the compiler do your debugging.

## The Cost of Weak Types

Consider this function signature:

```cpp
void schedule_meeting(std::string organizer, std::string room,
                      int hour, int minute, int duration);
```

How many bugs hide here? A caller can swap `organizer` and `room`. They can pass `hour: 25` or `minute: -3` or `duration: 0`. The compiler accepts all of it. You've pushed validation into runtime — into unit tests, if you're disciplined, or into production crashes, if you're not.

The type-theoretic response: make the wrong call *impossible to write*.

## Strong Types: The Foundation

A strong type wraps a primitive and gives it a unique identity in the type system. Two `std::string` values are interchangeable; an `Organizer` and a `RoomId` are not.

```cpp
template<typename T, typename Tag>
class StrongType
{
public:
    explicit StrongType(T value) : value_(std::move(value)) {}

    const T& get() const { return value_; }
    T& get() { return value_; }

private:
    T value_;
};
```

The `Tag` parameter is a phantom type — it exists only at the type level, never at runtime. It's a zero-cost proof that two values have different semantic meanings:

```cpp
struct OrganizerTag {};
struct RoomIdTag {};
struct HourTag {};

using Organizer = StrongType<std::string, OrganizerTag>;
using RoomId    = StrongType<std::string, RoomIdTag>;
using Hour      = StrongType<int, HourTag>;
```

Now this is a compile error:

```cpp
void schedule_meeting(Organizer org, RoomId room, Hour hour);

// Error: cannot convert RoomId to Organizer
schedule_meeting(RoomId("conf-a"), Organizer("alice"), Hour(10));
```

The compiler just proved your arguments are in the right order. No test needed.

### Adding Invariants to Strong Types

Phantom types distinguish values. But what about *valid* values? We can fold validation into construction:

```cpp
template<typename T, typename Tag, auto Validate>
class BoundedType
{
public:
    static std::optional<BoundedType> make(T value)
    {
        if (!Validate(value)) return std::nullopt;
        return BoundedType(std::move(value));
    }

    const T& get() const { return value_; }

private:
    explicit BoundedType(T value) : value_(std::move(value)) {}
    T value_;
};

// Validation as a compile-time lambda
constexpr auto valid_hour = [](int h) { return h >= 0 && h < 24; };
constexpr auto valid_minute = [](int m) { return m >= 0 && m < 60; };

using Hour   = BoundedType<int, struct HourTag, valid_hour>;
using Minute = BoundedType<int, struct MinuteTag, valid_minute>;
```

Now `Hour::make(25)` returns `std::nullopt`. The type *encodes* the invariant. If you have an `Hour`, it is valid — by construction, not by convention.

```cpp
void schedule(Hour h, Minute m)
{
    // No validation needed here. The types are the proof.
    std::cout << h.get() << ":" << m.get() << "\n";
}

auto h = Hour::make(14);    // optional<Hour>
auto m = Minute::make(30);  // optional<Minute>

if (h && m)
    schedule(*h, *m);  // Guaranteed valid
```

## Sum Types: Modeling "One Of"

A product type (`struct`) says "this AND that". A sum type says "this OR that". In type theory, products are conjunction; sums are disjunction. C++17 gave us `std::variant` — a proper sum type.

### The Problem with Enums + Conditionals

```cpp
enum class Shape { Circle, Rectangle, Triangle };

double area(Shape s, double a, double b)
{
    switch (s)
    {
        case Shape::Circle:    return 3.14159 * a * a;
        case Shape::Rectangle: return a * b;
        case Shape::Triangle:  return 0.5 * a * b;
    }
}
```

What does `a` mean for a circle? What stops you from passing `b` for a circle? Nothing. The enum + loose parameters pattern is a type-theoretic antipattern: it uses runtime branching where the type system could carry the information.

### Variants as Proper Sum Types

```cpp
struct Circle    { double radius; };
struct Rectangle { double width, height; };
struct Triangle  { double base, height; };

using Shape = std::variant<Circle, Rectangle, Triangle>;
```

Each arm carries exactly the data it needs. No spurious parameters. Now pattern match with `std::visit`:

```cpp
double area(const Shape& shape)
{
    return std::visit([](const auto& s) -> double
    {
        using T = std::decay_t<decltype(s)>;

        if constexpr (std::is_same_v<T, Circle>)
            return 3.14159 * s.radius * s.radius;
        else if constexpr (std::is_same_v<T, Rectangle>)
            return s.width * s.height;
        else if constexpr (std::is_same_v<T, Triangle>)
            return 0.5 * s.base * s.height;
    }, shape);
}
```

But this is verbose. The overload pattern is cleaner:

```cpp
template<typename... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

double area(const Shape& shape)
{
    return std::visit(overloaded{
        [](const Circle& c)    { return 3.14159 * c.radius * c.radius; },
        [](const Rectangle& r) { return r.width * r.height; },
        [](const Triangle& t)  { return 0.5 * t.base * t.height; },
    }, shape);
}
```

**The key property**: if you add a new variant arm (say `Ellipse`), every `std::visit` call that doesn't handle it becomes a compile error. The compiler enforces exhaustive case analysis. This is the sum-type guarantee — you cannot forget a case.

### Real-World Sum Types: Error Handling

`std::variant` replaces error codes, exception spaghetti, and sentinel values:

```cpp
struct ParseError  { int line; std::string message; };
struct TypeError   { std::string expected, got; };
struct RuntimeError { std::string message; };

using CompileError = std::variant<ParseError, TypeError, RuntimeError>;

std::string format_error(const CompileError& err)
{
    return std::visit(overloaded{
        [](const ParseError& e)   { return "Parse error at line " + std::to_string(e.line) + ": " + e.message; },
        [](const TypeError& e)    { return "Type error: expected " + e.expected + ", got " + e.got; },
        [](const RuntimeError& e) { return "Runtime error: " + e.message; },
    }, err);
}
```

Compare this to `catch (...)` hierarchies. The variant makes the error space *visible* in the function signature. A caller knows exactly what can go wrong.

### Accumulating Over Variants

You can fold variants into aggregate state, treating them as events in an event-sourced system:

```cpp
struct PostsChanged  { std::vector<std::string> paths; };
struct ConfigChanged {};
struct ThemeChanged  {};

using ChangeEvent = std::variant<PostsChanged, ConfigChanged, ThemeChanged>;

struct ChangeSet
{
    bool posts = false, config = false, theme = false;

    ChangeSet& operator<<(const ChangeEvent& ev)
    {
        std::visit(overloaded{
            [&](const PostsChanged&)  { posts = true; },
            [&](const ConfigChanged&) { config = true; },
            [&](const ThemeChanged&)  { theme = true; },
        }, ev);
        return *this;
    }
};
```

This is a fold over a sum type — the functional programming pattern, in plain C++.

## Concepts: Propositions About Types

C++20 concepts are the biggest type-theoretic leap the language has taken. A concept is a predicate on types — a proposition that a type must satisfy. The compiler checks it at instantiation.

### From Duck Typing to Structural Contracts

Before concepts, templates were duck-typed. If it compiled, it worked. If it didn't, you got 200 lines of template error noise. Concepts fix this:

```cpp
template<typename T>
concept Printable = requires(T value, std::ostream& os)
{
    { os << value } -> std::same_as<std::ostream&>;
};

void log(Printable auto const& value)
{
    std::cout << value << "\n";
}
```

`Printable` is a proposition: "type T supports streaming to an ostream." The compiler checks it *at the call site*, not deep inside the template body. Error messages become human-readable.

### Concepts as Interface Contracts

Concepts replace abstract base classes for many use cases — without vtable overhead:

```cpp
template<typename T>
concept ContentSource = requires(T source)
{
    { source.all_posts() } -> std::same_as<std::vector<Post>>;
    { source.all_pages() } -> std::same_as<std::vector<Page>>;
};

template<typename S>
concept Reloadable = requires(S source, const ChangeSet& cs)
{
    { source.reload(cs) } -> std::same_as<void>;
};
```

Any type that has `all_posts()` and `all_pages()` with the right signatures satisfies `ContentSource`. No inheritance. No `virtual`. No runtime dispatch. The concept is checked at compile time, and the call is a direct function call — zero overhead.

```cpp
template<ContentSource Source>
Site build_site(Source& source, SiteConfig config)
{
    return Site{
        .posts = source.all_posts(),
        .pages = source.all_pages(),
        // ...
    };
}
```

Both `FileSystemSource` and `MemorySource` satisfy this concept. The compiler verifies it. If someone writes a `DatabaseSource` that forgets `all_pages()`, they get a clear error at the call site, not a linker error.

### Composing Concepts

Concepts compose with `&&` and `||`, just like logical propositions:

```cpp
template<typename T>
concept WatchableSource = ContentSource<T> && Reloadable<T>;
```

This is conjunction in propositional logic: `WatchableSource(T) ⟺ ContentSource(T) ∧ Reloadable(T)`.

You can use `requires` clauses to add ad-hoc constraints:

```cpp
template<typename Source, typename Watcher, typename Cache>
    requires ContentSource<Source> && Reloadable<Source> && WatchPolicy<Watcher>
class HotReloader
{
    // The compiler has proven:
    // 1. Source can provide posts and pages
    // 2. Source can reload itself
    // 3. Watcher knows how to detect changes
    // This class cannot be instantiated with wrong types.
};
```

### static_assert as Proof

You can verify concept satisfaction at the point of definition:

```cpp
class InotifyWatcher { /* ... */ };

static_assert(WatchPolicy<InotifyWatcher>);
```

This is a compile-time proof obligation. If `InotifyWatcher` doesn't satisfy `WatchPolicy`, the build fails with a clear message pointing at this line. It's a type-level unit test.

## Product Types: Designing with Structs

Product types are the dual of sum types. Where a variant says "one of these," a struct says "all of these." In type theory, the number of possible values of a product type is the *product* of its fields' value spaces.

### Reducing the State Space

Consider:

```cpp
struct Connection
{
    std::string host;
    int port;
    bool connected;
    int socket_fd;     // Only valid when connected == true
    std::string error; // Only valid when connected == false
};
```

This type allows states like `{connected: true, socket_fd: -1, error: "timeout"}` — which is nonsensical. The product type is too large; it admits illegal states.

Fix it with a sum type:

```cpp
struct Connected
{
    std::string host;
    int port;
    int socket_fd;
};

struct Disconnected
{
    std::string host;
    int port;
    std::string error;
};

using Connection = std::variant<Connected, Disconnected>;
```

Now every possible value of `Connection` is a *valid* state. The illegal states are literally unrepresentable.

### Designated Initializers

C++20 designated initializers make product types self-documenting:

```cpp
struct SiteConfig
{
    std::string title;
    std::string description;
    std::string author;
    Navigation navigation;
    Theme theme;
    Footer footer;
};

auto config = SiteConfig{
    .title = "My Blog",
    .description = "Thoughts on type theory",
    .author = "Alice",
    .navigation = { /* ... */ },
    .theme = { /* ... */ },
    .footer = { /* ... */ },
};
```

Every field is named at the call site. No positional ambiguity. If you reorder fields in the struct, every initialization that uses the wrong order becomes a compile error.

## CRTP: Compile-Time Polymorphism

The Curiously Recurring Template Pattern lets you write polymorphic code without virtual dispatch. It's a type-level trick: a base class is parameterized on its derived class.

```cpp
template<typename Derived>
class Serializable
{
public:
    std::string serialize() const
    {
        // The base class calls into the derived class at compile time
        return static_cast<const Derived*>(this)->do_serialize();
    }
};

class User : public Serializable<User>
{
    std::string name_;
    int age_;

    friend class Serializable<User>;
    std::string do_serialize() const
    {
        return "{\"name\":\"" + name_ + "\",\"age\":" + std::to_string(age_) + "}";
    }
};
```

No vtable. No indirection. The compiler resolves `do_serialize()` at compile time. The "polymorphism" is entirely in the type system.

With C++20 concepts, you can often replace CRTP entirely:

```cpp
template<typename T>
concept Serializable = requires(const T& value)
{
    { value.serialize() } -> std::same_as<std::string>;
};

// Any type with a serialize() method works — no base class needed
void save(const Serializable auto& value)
{
    write_to_disk(value.serialize());
}
```

Prefer concepts over CRTP when you don't need the base class to provide default implementations.

## constexpr and consteval: Proofs at Compile Time

`constexpr` functions can run at compile time. `consteval` functions *must* run at compile time. This lets you push computation — and validation — into the compiler:

```cpp
consteval int factorial(int n)
{
    if (n < 0) throw "negative factorial"; // Compile error if triggered
    int result = 1;
    for (int i = 2; i <= n; ++i)
        result *= i;
    return result;
}

// Computed at compile time. No runtime cost.
constexpr int f10 = factorial(10); // 3628800

// This would fail to compile:
// constexpr int bad = factorial(-1);
```

### Compile-Time String Validation

```cpp
consteval bool is_valid_slug(std::string_view slug)
{
    if (slug.empty()) return false;
    for (char c : slug)
    {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
            return false;
    }
    return true;
}

template<size_t N>
struct Slug
{
    char data[N];

    consteval Slug(const char (&str)[N])
    {
        if (!is_valid_slug(std::string_view(str, N - 1)))
            throw "invalid slug";
        for (size_t i = 0; i < N; ++i)
            data[i] = str[i];
    }
};

// Valid — compiles
auto s1 = Slug("hello-world");

// Invalid — compile error: "invalid slug"
// auto s2 = Slug("Hello World!");
```

The slug validation runs entirely in the compiler. Invalid slugs are rejected before any code is generated. This is a compile-time proof of well-formedness.

## Template Metaprogramming as Logic

At its core, template metaprogramming is a logic programming language embedded in C++. Types are terms, templates are functions, specialization is pattern matching, and `static_assert` is proof obligation.

### Type-Level Lists

```cpp
template<typename... Ts>
struct TypeList {};

// Head: extract first element
template<typename List>
struct Head;

template<typename T, typename... Ts>
struct Head<TypeList<T, Ts...>>
{
    using type = T;
};

// Contains: check membership
template<typename List, typename T>
struct Contains;

template<typename T>
struct Contains<TypeList<>, T> : std::false_type {};

template<typename T, typename... Ts>
struct Contains<TypeList<T, Ts...>, T> : std::true_type {};

template<typename U, typename T, typename... Ts>
struct Contains<TypeList<U, Ts...>, T> : Contains<TypeList<Ts...>, T> {};

static_assert(Contains<TypeList<int, double, char>, double>::value);
static_assert(!Contains<TypeList<int, double, char>, float>::value);
```

These `static_assert` lines are proofs. The compiler is a theorem prover. If the assertion fails, your "theorem" is wrong.

### Type-Level State Machines

You can encode state machine transitions in the type system so that invalid transitions don't compile:

```cpp
struct Locked {};
struct Unlocked {};

template<typename State>
class Door
{
public:
    // Can only unlock a locked door
    Door<Unlocked> unlock() requires std::same_as<State, Locked>
    {
        return {};
    }

    // Can only lock an unlocked door
    Door<Locked> lock() requires std::same_as<State, Unlocked>
    {
        return {};
    }

    // Can only open an unlocked door
    void open() requires std::same_as<State, Unlocked>
    {
        // ...
    }
};

Door<Locked> door;
// door.open();              // Compile error!
auto unlocked = door.unlock();
unlocked.open();             // OK
auto relocked = unlocked.lock();
// relocked.open();          // Compile error!
```

The type system tracks the door's state. Calling `open()` on a `Door<Locked>` is a *type error*. The state machine is enforced at compile time with zero runtime overhead.

## Monadic Composition with std::optional and std::expected

C++23 gives `std::optional` and `std::expected` monadic operations (`and_then`, `transform`, `or_else`). Even in C++20, you can build this pattern:

```cpp
template<typename T>
class Result
{
public:
    static Result ok(T value) { return Result(std::move(value)); }
    static Result err(std::string msg) { return Result(std::move(msg)); }

    template<typename F>
    auto and_then(F&& f) -> std::invoke_result_t<F, T>
    {
        if (value_) return f(std::move(*value_));
        return std::invoke_result_t<F, T>::err(std::move(error_));
    }

    template<typename F>
    auto transform(F&& f) -> Result<std::invoke_result_t<F, T>>
    {
        using U = std::invoke_result_t<F, T>;
        if (value_) return Result<U>::ok(f(std::move(*value_)));
        return Result<U>::err(std::move(error_));
    }

private:
    Result(T value) : value_(std::move(value)) {}
    Result(std::string error) : error_(std::move(error)) {}

    std::optional<T> value_;
    std::string error_;
};
```

Now you can chain operations that might fail without nested `if` statements:

```cpp
auto result = parse_config(text)
    .and_then(validate_config)
    .transform([](Config c) { return build_site(c); });
```

Each step short-circuits on error. No exceptions. No error codes. The type system tracks success/failure through the chain.

## Putting It All Together: A Real System

Here's how all these ideas combine in a real hot-reload system. The types form a closed proof:

```cpp
// 1. Sum type: what changed
using ChangeEvent = std::variant<PostsChanged, PagesChanged, ConfigChanged, ThemeChanged>;

// 2. Product type: accumulated changes (fold over the sum type)
struct ChangeSet
{
    bool posts = false, pages = false, config = false, theme = false;

    ChangeSet& operator<<(const ChangeEvent& ev) { /* visit + fold */ }
    ChangeSet operator|(const ChangeSet& other) const { /* merge */ }
};

// 3. Concepts: structural contracts
template<typename T>
concept ContentSource = requires(T s) {
    { s.all_posts() } -> std::same_as<std::vector<Post>>;
    { s.all_pages() } -> std::same_as<std::vector<Page>>;
};

template<typename T>
concept Reloadable = requires(T s, const ChangeSet& cs) {
    { s.reload(cs) } -> std::same_as<void>;
};

template<typename W>
concept WatchPolicy = requires(W w) {
    { w.poll()  } -> std::same_as<std::optional<ChangeSet>>;
    { w.start() } -> std::same_as<void>;
    { w.stop()  } -> std::same_as<void>;
};

// 4. Constrained template: the compiler proves all three contracts
template<typename Source, WatchPolicy Watcher, typename Cache>
    requires ContentSource<Source> && Reloadable<Source>
class HotReloader { /* ... */ };

// 5. static_assert: proof obligations at definition site
static_assert(WatchPolicy<InotifyWatcher>);
static_assert(ContentSource<FileSystemSource>);
static_assert(Reloadable<FileSystemSource>);
```

When you instantiate `HotReloader<FileSystemSource, InotifyWatcher, SiteCache>`, the compiler verifies:
- `FileSystemSource` provides posts and pages (content contract)
- `FileSystemSource` can reload itself (reload contract)
- `InotifyWatcher` can detect changes (watch contract)

If any contract is violated, you get a clear, concept-based error at the instantiation site. Not a 500-line template backtrace. A one-line message saying which requirement failed.

## The Type-Theoretic Mindset

Here's the mental shift:

1. **Don't validate — construct.** Instead of checking if data is valid, make invalid data unconstructable. Use factory functions that return `optional` or `expected`.

2. **Don't branch — dispatch.** Instead of `if (type == X)`, use `std::visit` on a variant. The compiler ensures you handle every case.

3. **Don't document — constrain.** Instead of comments saying "T must have a serialize method," write a concept. The compiler enforces what comments cannot.

4. **Don't test invariants — encode them.** If an hour must be 0-23, don't test it — make the type reject 24. Tests verify behavior; types verify structure.

5. **Don't inherit — compose.** Prefer concepts over abstract base classes. Prefer variant over enum + switch. Prefer strong types over aliases.

The compiler is the most thorough code reviewer you'll ever have. It runs every time you build, it never gets tired, and it checks every code path. Type-theoretic programming is the practice of giving it more to check.

Write types that make wrong code look wrong — not to humans, but to the compiler. Then let it do the proving.
