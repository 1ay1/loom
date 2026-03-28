---
title: "std::variant and std::visit — When a Value Can Be One of Many Things"
date: 2025-10-10
slug: variant-visit-and-sum-types
tags: cpp, variant, visit, sum-types, pattern-matching
excerpt: A variant says "I am exactly one of these types, and you must handle every possibility." The compiler enforces it.
---

Most programming decisions fall into two categories: "this thing has all of these properties" (a struct) and "this thing is one of several possibilities" (a variant). Structs are product types — the total information is the *product* of all fields. Variants are sum types — the total information is the *sum* of all alternatives.

C++ has had structs forever. It gained sum types in C++17 with `std::variant`.

## The Problem: Content Changes

Loom's hot reloader needs to know what changed on disk. A file change can be one of several things:

- Posts changed (one or more markdown files in the posts directory)
- Pages changed (one or more files in the pages directory)
- Config changed (the config file was modified)
- Theme changed (a theme file was modified)

These are mutually exclusive categories with different data. A `PostsChanged` carries the list of affected file paths. A `ConfigChanged` carries nothing — just the fact that config changed. This is a perfect fit for a variant.

## Loom's ChangeEvent

```cpp
struct PostsChanged  { std::vector<std::string> paths; };
struct PagesChanged  { std::vector<std::string> paths; };
struct ConfigChanged {};
struct ThemeChanged  {};

using ChangeEvent = std::variant<PostsChanged, PagesChanged, ConfigChanged, ThemeChanged>;
```

A `ChangeEvent` is exactly one of these four types. Not "maybe any of them." Not "some base class pointer that could be anything." Exactly one, and the type system knows which one.

This is different from inheritance. With inheritance, you have a pointer to a base class and you don't know the concrete type without a dynamic cast or virtual dispatch. With a variant, the type is stored alongside the value, and the compiler ensures you handle every case.

## Creating Variants

Creating a variant is straightforward — you just assign one of the alternatives:

```cpp
ChangeEvent event = PostsChanged{{"content/posts/hello.md"}};
// event holds a PostsChanged

event = ConfigChanged{};
// event now holds a ConfigChanged
```

Each alternative is a distinct struct. The variant's storage is the size of the largest alternative plus a small tag (typically 1-4 bytes) that identifies which alternative is active.

## holds_alternative and get

You can query which alternative a variant holds:

```cpp
if (std::holds_alternative<PostsChanged>(event)) {
    const auto& changed = std::get<PostsChanged>(event);
    for (const auto& path : changed.paths) {
        std::cout << "Post changed: " << path << "\n";
    }
}
```

`holds_alternative` returns true if the variant currently holds the specified type. `get<T>` extracts the value, throwing `std::bad_variant_access` if the variant holds a different type.

But this pattern — check then get — is fragile. If you add a fifth variant alternative later, you have to find every `holds_alternative` chain and add a new branch. The compiler won't warn you about the missing case.

## std::visit: The Right Way

`std::visit` calls a visitor for the active alternative. If the visitor doesn't handle every alternative, the code doesn't compile:

```cpp
std::visit([](const auto& event) {
    using T = std::decay_t<decltype(event)>;
    if constexpr (std::is_same_v<T, PostsChanged>) {
        std::cout << "Posts changed: " << event.paths.size() << " files\n";
    } else if constexpr (std::is_same_v<T, PagesChanged>) {
        std::cout << "Pages changed\n";
    } else if constexpr (std::is_same_v<T, ConfigChanged>) {
        std::cout << "Config changed\n";
    } else if constexpr (std::is_same_v<T, ThemeChanged>) {
        std::cout << "Theme changed\n";
    }
}, event);
```

The generic lambda receives the concrete type — no downcasting, no virtual dispatch. The `if constexpr` chain handles each case at compile time. But this approach is verbose.

## The Overloaded Pattern

A cleaner approach is the overloaded lambda pattern:

```cpp
template<typename... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

// Usage:
std::visit(overloaded{
    [](const PostsChanged& e) { /* handle posts */ },
    [](const PagesChanged& e) { /* handle pages */ },
    [](const ConfigChanged&)  { /* handle config */ },
    [](const ThemeChanged&)   { /* handle theme */ },
}, event);
```

The `overloaded` struct inherits from all the lambdas and pulls in all their `operator()` overloads. `std::visit` picks the right lambda based on the variant's active type. If you forget to handle one of the alternatives, the compiler tells you.

This is the closest C++ gets to pattern matching. Each lambda handles one case. The compiler verifies exhaustiveness. The dispatch is efficient — typically a jump table or if-else chain, not a virtual call.

## Loom's ChangeSet: Folding Over Variants

Loom doesn't use `std::visit` directly for change events. Instead, it folds them into a `ChangeSet` using a visitor struct:

```cpp
struct ChangeSet {
    bool posts  = false;
    bool pages  = false;
    bool config = false;
    bool theme  = false;

    ChangeSet& operator<<(const ChangeEvent& ev) {
        std::visit(Absorb{*this}, ev);
        return *this;
    }

private:
    struct Absorb {
        ChangeSet& cs;
        void operator()(const PostsChanged&)  { cs.posts  = true; }
        void operator()(const PagesChanged&)  { cs.pages  = true; }
        void operator()(const ConfigChanged&) { cs.config = true; }
        void operator()(const ThemeChanged&)  { cs.theme  = true; }
    };
};
```

The `Absorb` struct is a visitor with four overloads. `std::visit` calls the right one based on which alternative the `ChangeEvent` holds. The `operator<<` provides a convenient syntax for folding events:

```cpp
ChangeSet changes;
changes << PostsChanged{{"a.md", "b.md"}};
changes << ConfigChanged{};
// changes.posts == true, changes.config == true
```

Multiple events can be folded:

```cpp
changes << events;  // events is a vector<ChangeEvent>
```

This pattern — variant as input, visitor as fold, accumulator as output — is a functional programming pattern expressed in C++. The variant ensures every event type is handled. The visitor ensures the handling is type-safe. The accumulator collects the results.

## Variant vs. Inheritance

Why use a variant instead of an inheritance hierarchy?

With inheritance:

```cpp
struct ChangeEvent {
    virtual ~ChangeEvent() = default;
    virtual void apply(ChangeSet& cs) const = 0;
};

struct PostsChanged : ChangeEvent {
    std::vector<std::string> paths;
    void apply(ChangeSet& cs) const override { cs.posts = true; }
};
```

With variant:

```cpp
using ChangeEvent = std::variant<PostsChanged, PagesChanged, ConfigChanged, ThemeChanged>;
```

The variant approach wins on several axes:

**Closed set.** A variant's alternatives are fixed at definition time. You can't secretly add a fifth alternative in a different translation unit. This means the compiler can verify exhaustive handling. With inheritance, anyone can add a subclass anywhere.

**Value semantics.** Variants are values — they can be copied, moved, stored in vectors, passed by value. Inheritance-based polymorphism requires pointers or references, which means heap allocation, indirection, and lifetime management.

**No virtual dispatch.** Variant dispatch is a switch or if-else chain on a small integer tag. Virtual dispatch goes through a vtable pointer, which can cause cache misses. For hot paths, the variant approach is faster.

**Memory layout.** A variant is a single contiguous block of memory (max alternative size + tag). An inheritance hierarchy requires base class pointers, vtable pointers, and potentially scattered heap allocations.

**When to use inheritance instead:** When the set of types is open (plugins, extensible systems), when types have significantly different sizes, or when you need runtime polymorphism across compilation boundaries.

Loom uses variants for change events (a closed set of known categories) and concepts for extension points (content sources, watch policies — open sets that users can implement).

## Variant as State Machine

Variants are also natural state machine representations:

```cpp
struct Idle {};
struct Connecting { std::string host; int port; };
struct Connected { int socket_fd; };
struct Error { std::string message; };

using ConnectionState = std::variant<Idle, Connecting, Connected, Error>;
```

Each state carries exactly the data it needs. An `Idle` connection has no socket. A `Connected` connection has a file descriptor. An `Error` has a message. You can't accidentally access the socket of a connection that's still connecting — the type system prevents it.

State transitions are variant reassignments:

```cpp
ConnectionState state = Idle{};
state = Connecting{"localhost", 8080};
// ... after connecting succeeds:
state = Connected{socket_fd};
// ... or on failure:
state = Error{"Connection refused"};
```

## The Bigger Picture

Variants are how C++ does algebraic data types. Combined with `std::visit`, they give you exhaustive pattern matching over a closed set of types. Combined with structs, they let you model data precisely — each alternative carries exactly the data it needs, no more.

Loom's change event system is a textbook example: four event types with different data, folded into a boolean flag set via a visitor. The variant makes the events type-safe. The visitor makes the handling exhaustive. The ChangeSet makes the accumulated state simple to query.

This pattern — variant input, visitor processing, structured output — appears everywhere in well-designed C++. Parsers produce variants of token types. State machines are variants of states. Error handling uses variants of success and error types. Once you start thinking in sum types, you see them everywhere.

Next: atomics and memory ordering — where we leave the type system and enter the hardware.
