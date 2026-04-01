---
title: "Concepts as Interfaces — The Type System of the Compile-Time Language"
date: 2026-02-25
slug: concepts-as-interfaces
tags: compile-time-cpp, concepts, constraints, subsumption, requires-clause
excerpt: Concepts aren't just prettier SFINAE. They're the type system of the compile-time language — named, composable interfaces that tell the compiler (and the reader) what a type must be.
---

Previous posts used concepts for two things: compile-time branching ([Control Flow](/post/control-flow-at-compile-time)) and error messages ([Diagnostics](/post/error-messages-and-diagnostics)). Both are real uses. Neither captures the full picture.

Concepts are the type system of the compile-time language.

That sentence needs unpacking. The runtime language has a type system: every variable has a type, every function signature specifies parameter types, and the compiler checks that everything matches. The compile-time language — the one where templates are functions and types are values — didn't have a proper type system until C++20. Templates accepted anything. If the body compiled, great. If not, you got eighty lines of error. There was no way to say "this template takes a type that is sortable" or "this template takes a type that looks like an iterator." You could only say "this template takes a type" and hope for the best.

Concepts fill that gap. A concept is a named predicate on types — a compile-time boolean that says whether a type qualifies. But more importantly, a concept is a *contract*. It documents what the template requires. It enforces requirements at the boundary. It participates in overload resolution. And it composes, just like types compose in the runtime language.

This post treats concepts as what they are: interfaces for the compile-time language. Not syntax sugar for SFINAE. Not just error message improvers. A complete interface system.

## The Anatomy of a Concept

A concept is a `bool` expression parameterized by one or more types. It can combine type traits, `requires` expressions, and other concepts with `&&` and `||`.

```cpp
template<typename T>
concept Numeric = std::is_arithmetic_v<T>;
```

This is the simplest form. `Numeric<int>` is `true`. `Numeric<std::string>` is `false`. It's a named boolean predicate.

The `requires` expression is where concepts get expressive. It lets you check whether specific expressions are valid for a type:

```cpp
template<typename T>
concept Hashable = requires(T a) {
    { std::hash<T>{}(a) } -> std::convertible_to<std::size_t>;
};
```

This says: "a type `T` is `Hashable` if you can construct a `std::hash<T>`, call it with a value of type `T`, and the result is convertible to `std::size_t`." It doesn't check this by instantiating `std::hash` and seeing if it compiles. It checks it structurally — asking whether the expression would be valid, without actually evaluating it. If the expression is ill-formed, the concept evaluates to `false`. No substitution failure. No error cascade.

You can check multiple requirements:

```cpp
template<typename T>
concept Container = requires(T c) {
    typename T::value_type;                         // has a nested value_type
    typename T::iterator;                           // has a nested iterator
    { c.begin() } -> std::same_as<typename T::iterator>;
    { c.end() } -> std::same_as<typename T::iterator>;
    { c.size() } -> std::convertible_to<std::size_t>;
    { c.empty() } -> std::same_as<bool>;
};
```

Six requirements. Each one is independently checkable. If `T` fails any of them, the concept is `false`, and the compiler can tell you *which* one failed. Compare this with pre-C++20, where you'd either write six `enable_if` conditions or let the template body fail at whatever happened to be the first invalid expression.

## Concepts Are Not Just Constraints

The SFINAE mental model says: "concepts are constraints that filter overloads." That's true but limited. It's like saying "types are things that make the compiler check your code." Technically correct, functionally reductive.

Here's the richer view. In the runtime language, an interface (abstract class, protocol, trait — whatever your language calls it) serves three purposes:

1. **Documentation**: it tells readers what the type can do
2. **Enforcement**: it prevents misuse at the boundary
3. **Dispatch**: it enables polymorphic selection

Concepts serve all three purposes in the compile-time language:

**Documentation:**

```cpp
template<std::ranges::random_access_range R>
void sort(R& range);
```

The concept name `random_access_range` tells you everything. This function needs random access. Don't pass a `std::forward_list`. You don't need to read the implementation to know this.

**Enforcement:**

```cpp
sort(my_list);  // error: std::list does not satisfy random_access_range
```

The error is at the call site. One line. No template instantiation backtrace. The boundary caught the problem.

**Dispatch:**

```cpp
template<std::ranges::random_access_range R>
void fast_sort(R& range) { /* use nth_element + partition */ }

template<std::ranges::bidirectional_range R>
void fast_sort(R& range) { /* use merge sort */ }

template<std::ranges::forward_range R>
void fast_sort(R& range) { /* use selection sort */ }
```

Three overloads. The compiler picks the most constrained one that the argument satisfies. A `std::vector` gets the random access version. A `std::list` gets the bidirectional version. A `std::forward_list` gets the forward version. This is compile-time polymorphism through concept-based dispatch.

## Subsumption: The Concept Hierarchy

When multiple concepts match, how does the compiler decide which overload wins? Through **subsumption** — the partial ordering of constraints.

Concept A subsumes concept B if A logically implies B. If every type that satisfies A also satisfies B, then A is "more constrained" and wins in overload resolution.

```cpp
template<typename T>
concept Movable = std::is_move_constructible_v<T> && std::is_move_assignable_v<T>;

template<typename T>
concept Copyable = Movable<T> &&
    std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>;
```

`Copyable` subsumes `Movable` because `Copyable` includes everything `Movable` requires, plus more. A function constrained on `Copyable` is more specific than one constrained on `Movable`, so it wins when both match:

```cpp
template<Movable T>
void store(T value) { /* move into storage */ }

template<Copyable T>
void store(T value) { /* copy into storage, keep original */ }
```

For a `std::unique_ptr` (movable but not copyable), the first overload is selected. For a `std::string` (copyable), the second wins because `Copyable` subsumes `Movable`.

This is the compile-time equivalent of virtual dispatch — except the dispatch is resolved entirely at compile time, with zero runtime overhead. No vtable. No indirection. Just the right function, selected by the compiler based on the type's capabilities.

### How Subsumption Works (The Constraints Normalized Form)

Subsumption isn't magic. The compiler breaks each concept into its **atomic constraints** — the individual boolean expressions connected by `&&` and `||`. Then it checks: does every disjunctive normal form clause of A imply some clause of B?

The key gotcha: subsumption only works when concepts are composed *syntactically* using `&&` and `||`. If you hide the same logic inside a trait, subsumption breaks:

```cpp
// Subsumption works:
template<typename T>
concept A = std::integral<T>;

template<typename T>
concept B = A<T> && sizeof(T) >= 4;
// B subsumes A — the compiler can see that B includes A's constraint.

// Subsumption breaks:
template<typename T>
constexpr bool is_big_integral = std::integral<T> && sizeof(T) >= 4;

template<typename T>
concept C = is_big_integral<T>;
// C does NOT subsume A — the compiler sees is_big_integral<T> as an opaque
// boolean, not as a conjunction containing A's constraint.
```

The lesson: when building concept hierarchies, compose concepts from other concepts using `&&`. Don't hide the relationships inside helper functions or traits, or the subsumption machinery won't see them.

## Designing Concept Hierarchies

Good concept hierarchies mirror the structure of your domain. They start broad and narrow progressively.

The standard library's iterator concepts are the canonical example:

```
input_or_output_iterator
├── input_iterator
│   ├── forward_iterator
│   │   ├── bidirectional_iterator
│   │   │   └── random_access_iterator
│   │   │       └── contiguous_iterator
│   │   └── ...
│   └── ...
└── output_iterator
```

Each level adds requirements. A `forward_iterator` is an `input_iterator` with multi-pass guarantees. A `bidirectional_iterator` is a `forward_iterator` that can go backwards. The hierarchy captures the progressive capability refinement that iterators exhibit.

Your own hierarchies should follow the same pattern:

```cpp
template<typename T>
concept Readable = requires(const T& t) {
    { t.read() } -> std::convertible_to<std::span<const std::byte>>;
};

template<typename T>
concept Writable = requires(T& t, std::span<const std::byte> data) {
    { t.write(data) } -> std::convertible_to<std::size_t>;
};

template<typename T>
concept ReadWrite = Readable<T> && Writable<T>;

template<typename T>
concept Seekable = ReadWrite<T> && requires(T& t, std::int64_t offset) {
    { t.seek(offset) };
    { t.tell() } -> std::convertible_to<std::int64_t>;
};
```

Each concept builds on the previous ones. `Seekable` subsumes `ReadWrite`, which subsumes both `Readable` and `Writable`. Functions constrained on `Seekable` are more specific than functions constrained on `ReadWrite`, which are more specific than those constrained on `Readable` alone. The compiler resolves all of this at overload resolution time.

## requires Expressions In Depth

The `requires` expression is the workhorse of concept definitions. It can check four kinds of things:

**Simple requirements** — whether an expression is valid:

```cpp
requires(T t) {
    t.foo();        // T must have a foo() method
    t + t;          // T must support operator+
    *t;             // T must be dereferenceable
}
```

**Type requirements** — whether a nested type exists:

```cpp
requires {
    typename T::value_type;
    typename T::iterator;
    typename std::hash<T>;    // std::hash must be specializable for T
}
```

**Compound requirements** — whether an expression is valid *and* its result satisfies a constraint:

```cpp
requires(T a, T b) {
    { a + b } -> std::same_as<T>;           // addition returns T
    { a < b } -> std::convertible_to<bool>; // comparison returns something bool-like
    { a.size() } noexcept;                  // size() must be noexcept
}
```

The `-> concept` part after the braces is a return type constraint. `{ expr } -> C<Args...>` checks that `C<decltype((expr)), Args...>` is satisfied. Note the double parentheses — it preserves the expression's value category.

**Nested requirements** — compound boolean conditions:

```cpp
requires {
    requires sizeof(T) <= 64;
    requires std::is_trivially_copyable_v<T>;
    requires Hashable<T>;
}
```

Nested `requires` evaluates a boolean expression. The outer `requires` clause is checking expression validity; the nested one is checking a boolean predicate. This distinction matters: without the nested `requires`, `sizeof(T) <= 64` would be a simple requirement (checking that the expression is valid, which it always is), not a check that the size is actually at most 64.

## Concepts vs. Inheritance: Structural vs. Nominal

Runtime interfaces in C++ use inheritance. A class `implements` an interface by inheriting from it and overriding virtual functions. This is **nominal** typing: the relationship is explicit, declared at the class definition.

Concepts use **structural** typing. A type satisfies a concept if it has the right structure — the right methods, the right nested types, the right operators. No inheritance required. No declaration required. The compiler checks structurally.

```cpp
template<typename T>
concept Drawable = requires(T& t, Canvas& c) {
    t.draw(c);
    { t.bounds() } -> std::same_as<Rect>;
};

// This satisfies Drawable without knowing Drawable exists:
struct Circle {
    void draw(Canvas& c) { /* ... */ }
    Rect bounds() { return {x - r, y - r, 2*r, 2*r}; }
    double x, y, r;
};
```

`Circle` never mentions `Drawable`. It satisfies `Drawable` because it has `draw(Canvas&)` and `bounds() -> Rect`. If someone later defines a `Drawable` concept, all existing types that happen to have the right interface automatically satisfy it. No refactoring needed.

This is the same structural typing that Go interfaces, Rust traits (with some caveats), and TypeScript interfaces use. It's retroactive compatibility: types satisfy interfaces they've never heard of, as long as they have the right shape.

The tradeoff is discoverability. With inheritance, you can look at a class definition and see which interfaces it implements. With concepts, the relationship is implicit — you need to check whether the type happens to satisfy the concept. Modern IDEs are getting better at this, but it's still a gap compared to explicit `implements` declarations.

## Concept-Based Overloading Patterns

### The Refinement Pattern

Provide a general implementation and a specialized one for types with additional capabilities:

```cpp
template<std::ranges::range R>
auto sum(const R& range) {
    // General: iterate and accumulate
    using T = std::ranges::range_value_t<R>;
    T total{};
    for (const auto& elem : range) {
        total += elem;
    }
    return total;
}

template<std::ranges::contiguous_range R>
    requires std::is_arithmetic_v<std::ranges::range_value_t<R>>
auto sum(const R& range) {
    // Optimized: use SIMD-friendly pointer arithmetic
    auto* begin = std::ranges::data(range);
    auto count = std::ranges::size(range);
    return optimized_sum(begin, count);
}
```

For a `std::vector<int>`, the second overload wins (contiguous + arithmetic). For a `std::list<int>`, the first wins (not contiguous). Same function name, different algorithms, selected at compile time by the type's capabilities.

### The Adapter Pattern

Use concepts to define what an adapter needs, then let users plug in any conforming type:

```cpp
template<typename T>
concept Logger = requires(T& logger, std::string_view message, int level) {
    logger.log(message, level);
    { logger.is_enabled(level) } -> std::same_as<bool>;
};

template<Logger L>
class Application {
    L logger_;
public:
    Application(L logger) : logger_(std::move(logger)) {}

    void run() {
        if (logger_.is_enabled(1)) {
            logger_.log("Application started", 1);
        }
        // ...
    }
};
```

Any type that has `log(string_view, int)` and `is_enabled(int) -> bool` works as a logger. A file logger. A network logger. A `/dev/null` logger that optimizes away. The concept defines the interface. The template accepts anything that satisfies it.

This is C++'s answer to dependency injection — without virtual dispatch overhead. The `Logger` type is baked into `Application` at compile time. The function calls are inlined. There's no vtable lookup. The abstraction is zero-cost.

### The Conditional Member Pattern

Provide additional methods only when the type supports them:

```cpp
template<typename T>
class SmartBuffer {
    std::vector<T> data_;
public:
    void push(T value) {
        data_.push_back(std::move(value));
    }

    std::size_t size() const { return data_.size(); }

    // Only available if T is printable
    void print() const requires requires(std::ostream& os, const T& t) { os << t; } {
        for (const auto& elem : data_) {
            std::cout << elem << ' ';
        }
        std::cout << '\n';
    }

    // Only available if T is sortable
    void sort() requires std::totally_ordered<T> {
        std::sort(data_.begin(), data_.end());
    }
};
```

`SmartBuffer<int>` has both `print()` and `sort()`. `SmartBuffer<NonPrintable>` has `sort()` but not `print()`. The constraints on individual methods let you build types that adapt their interface to their template argument.

## When Concepts Go Wrong

### Over-Constraining

The most common mistake is requiring more than you actually need:

```cpp
// Too strict: excludes types that would work fine
template<typename T>
concept StrictNumeric = std::is_arithmetic_v<T> && std::is_signed_v<T>;

template<StrictNumeric T>
T absolute_value(T x) { return x < 0 ? -x : x; }
```

This rejects `unsigned int`, which is perfectly fine for `absolute_value` (it would just return `x`). The concept enforces `is_signed`, but the implementation doesn't actually need signedness — it handles the unsigned case correctly via the conditional. Over-constraining turns valid uses into compile errors.

The fix: constrain only what you actually need. If the function body works with a type, the concept shouldn't reject it.

### Under-Constraining

The opposite problem: the concept is too loose, and errors surface inside the function body instead of at the boundary:

```cpp
template<typename T>
concept Printable = requires(T t) { std::cout << t; };

template<Printable T>
void pretty_print(const T& value) {
    std::cout << "[" << value.name() << ": " << value << "]";
    //                  ^^^^^^^^^^^^^ .name() isn't checked by the concept!
}
```

The concept checks that `T` can be printed, but the function also calls `.name()`. A type with `operator<<` but no `.name()` satisfies the concept but fails inside the function body. The concept should include all requirements:

```cpp
template<typename T>
concept PrettyPrintable = requires(const T& t, std::ostream& os) {
    { os << t };
    { t.name() } -> std::convertible_to<std::string_view>;
};
```

### Circular Concepts

You can't define concepts that depend on each other:

```cpp
template<typename T>
concept A = requires { requires B<T>; };  // error: B not defined yet

template<typename T>
concept B = requires { requires A<T>; };  // error: circular dependency
```

Concepts must form a DAG (directed acyclic graph). This is enforced syntactically — a concept can only reference concepts that were defined before it.

## The Standard Library's Concepts

C++20 shipped a comprehensive set of standard concepts in `<concepts>` and `<ranges>`. These aren't just examples — they're the vocabulary types of the compile-time interface system. Learning them is like learning the standard library: you don't have to, but you should, because everything builds on them.

**Core language concepts** (`<concepts>`):
- `same_as`, `derived_from`, `convertible_to` — type relationships
- `integral`, `floating_point`, `signed_integral`, `unsigned_integral` — arithmetic types
- `assignable_from`, `swappable`, `destructible` — operations
- `constructible_from`, `default_initializable`, `move_constructible`, `copy_constructible` — construction
- `equality_comparable`, `totally_ordered` — comparison
- `movable`, `copyable`, `semiregular`, `regular` — semantic bundles

**Iterator concepts** (`<iterator>`):
- `input_iterator`, `output_iterator`, `forward_iterator`, `bidirectional_iterator`, `random_access_iterator`, `contiguous_iterator` — the iterator hierarchy

**Range concepts** (`<ranges>`):
- `range`, `sized_range`, `view`, `input_range`, `forward_range`, `bidirectional_range`, `random_access_range`, `contiguous_range` — range classifications

**Callable concepts** (`<concepts>`):
- `invocable`, `regular_invocable`, `predicate`, `relation`, `equivalence_relation`, `strict_weak_order` — function-like types

These concepts compose. `regular` is `semiregular` + `equality_comparable`. `semiregular` is `copyable` + `default_initializable`. `copyable` is `copy_constructible` + `movable` + `assignable_from`. The hierarchy is deep and well-designed.

When you define your own concepts, build them on the standard ones. Don't redefine `std::integral` — use it. Don't recheck `std::copy_constructible` — compose with it. The standard concepts are the primitive types of the compile-time interface system. Your concepts are the compound types.

## Concepts as the Compile-Time Type System

Let's zoom out.

Before C++20, the compile-time language was dynamically typed. Templates accepted any argument. Type errors were discovered during instantiation — deep inside the implementation, often many template layers from the call site. This is the template equivalent of a Python `AttributeError`: it works until it doesn't, and when it doesn't, the error is far from the cause.

Concepts make the compile-time language statically typed. Template parameters are annotated with their required interface. Type errors are caught at the boundary — at the call site, before instantiation, with messages that name the violated requirement.

The runtime language went through a similar evolution. Early C had weak typing — anything could be cast to anything. C++ added static type checking. The code got safer. The errors got earlier. The documentation got better. Concepts are the same evolution, applied to the compile-time language.

Templates without concepts are the compile-time equivalent of `void*`. They work with anything. They tell you nothing. They fail deep inside. Templates with concepts are the compile-time equivalent of typed parameters. They tell you what they need. They reject mismatches immediately. They compose into hierarchies that model your domain.

Use them.
