---
title: "Phantom Types — Making the Compiler See What Isn't There"
date: 2026-03-23
slug: phantom-types
tags: [c++20, type-theory, phantom-types, templates, zero-cost, parametricity]
excerpt: "A phantom type parameter exists only in the type system — it carries no data, occupies no memory, and vanishes in the binary. Parametricity guarantees it works. Representation independence makes it free."
---

In the [last post](/post/pattern-matching), we saw how pattern matching deconstructs algebraic types — the elimination rule that mirrors construction, with exhaustiveness enforced by the compiler. But sometimes the distinction you need to enforce is not between different *data* — it is between different *meanings* of the same data.

A user ID and an order ID are both integers. A meters value and a feet value are both doubles. A validated string and an unvalidated string are both `std::string`. Structurally identical. Semantically incompatible.

We need the compiler to see a difference that does not exist in the data. The technique is called **phantom types** — type parameters that appear in the type signature but are never stored, never accessed, and never present at runtime. They are ghosts in the type system. And they are one of the most powerful tools in type-theoretic C++.

The deep reason phantom types work — the reason they are safe and zero-cost — comes from a property called **parametricity**. We will introduce it here and explore it fully in [part 8](/post/parametricity).

## The Problem: Structural Equality

The C++ type system is *structural* by default for built-in types. Two `int`s are the same type, regardless of what they represent:

```cpp
void book_flight(int passenger_id, int flight_id, int seat_number);
```

Three `int`s. The compiler sees three identical types. If you pass the arguments in the wrong order, the code compiles and silently does the wrong thing.

## The Phantom: A Tag That Vanishes

```cpp
template<typename Tag, typename T = int>
struct Strong {
    T value;

    explicit Strong(T v) : value(v) {}

    auto operator==(const Strong& other) const -> bool { return value == other.value; }
    auto operator<=>(const Strong& other) const = default;
};
```

The `Tag` parameter is the phantom. It appears in the type `Strong<Tag, T>` but is never stored, never accessed, never present in the object's memory layout. `sizeof(Strong<Tag, T>) == sizeof(T)`.

```cpp
struct PassengerTag {};
struct FlightTag {};
struct SeatTag {};

using PassengerId = Strong<PassengerTag>;
using FlightId = Strong<FlightTag>;
using SeatNumber = Strong<SeatTag>;

void book_flight(PassengerId passenger, FlightId flight, SeatNumber seat);
```

The phantom tag contributes zero bytes but creates entirely distinct types. Passing a `FlightId` where a `PassengerId` is expected is a compile error.

## Why Phantom Types Work: Parametricity

The theoretical foundation of phantom types is **parametric polymorphism** — the property that a function generic in a type parameter T must behave uniformly regardless of what T is. This was formalized by John Reynolds in 1983 and leads to one of the most beautiful results in type theory.

Consider a function operating on `Strong<Tag, int>`:

```cpp
template<typename Tag>
auto increment(Strong<Tag, int> s) -> Strong<Tag, int> {
    return Strong<Tag, int>{s.value + 1};
}
```

This function is *parametric* in `Tag`. It receives a `Tag` parameter but never inspects it — it cannot, because `Tag` is just a type with no values accessible to the function body. Parametricity guarantees that `increment` behaves identically for `PassengerTag`, `FlightTag`, or any other tag.

The crucial consequence: **the function cannot break tag safety**. It cannot convert a `Strong<PassengerTag>` into a `Strong<FlightTag>`. It cannot branch on what the tag is. It can only operate on the `int` inside and faithfully propagate the tag through. The tag flows through the type system like a watermark — visible in the types, invisible in the computation.

This is why phantom types are *safe*: the code cannot observe the tag, so it cannot violate the abstraction. And it is why they are *zero-cost*: there is nothing to observe at runtime.

We will formalize this as "free theorems" in [part 8](/post/parametricity): from the type signature alone, without reading the function body, you can prove that `increment` preserves the tag.

## Representation Independence

Because phantom-tagged types have identical runtime representation, you get a property from abstract type theory called **representation independence**: the observable behavior of phantom-typed code depends only on the underlying data, not on the phantom tag.

This connects to a deep idea from ML module systems. When a module exports a type abstractly — hiding its implementation — clients can only interact with it through the module's interface. Different implementations of the module produce different abstract types, but the behavior is consistent. Phantom types achieve the same effect without modules: `Strong<PassengerTag, int>` and `Strong<FlightTag, int>` are "different abstract types" with the same underlying representation.

The practical consequence: you can freely add new phantom-tagged types without any runtime cost or binary size increase. `using InvoiceId = Strong<struct InvoiceTag>` is one line, creates a fully distinct type, and compiles to the same machine code as a bare `int`.

## Shared Behavior via Templates

The template approach lets you write common operations once:

```cpp
template<typename Tag, typename T>
struct Strong {
    T value;
    explicit Strong(T v) : value(v) {}
    auto operator==(const Strong&) const -> bool = default;
    auto operator<=>(const Strong&) const = default;

    friend auto operator<<(std::ostream& os, const Strong& s) -> std::ostream& {
        return os << s.value;
    }
};

template<typename Tag, typename T>
struct std::hash<Strong<Tag, T>> {
    auto operator()(const Strong<Tag, T>& s) const -> size_t {
        return std::hash<T>{}(s.value);
    }
};
```

Every strong type gets comparison, ordering, streaming, and hashing for free. New types are one line:

```cpp
using InvoiceId = Strong<struct InvoiceTag>;
```

The `struct InvoiceTag` is defined inline — a forward declaration used only as a phantom. It never needs a body.

### Conditional Interfaces

With concepts, the template can expose different operations depending on the underlying type:

```cpp
template<typename Tag, typename T>
struct Strong {
    T value;

    auto operator+(const Strong& other) const -> Strong
        requires requires(T a, T b) { a + b; }
    {
        return Strong{value + other.value};
    }
};
```

ID types (tagged `int`) do not get arithmetic — you cannot "add" two user IDs. Measurement types (tagged `double`) do. Same template, different capabilities, driven by the tag and the underlying type. The `requires` clause is a formation rule on the method: it exists only when the underlying type supports the operation.

## Phantom Types as Proof Witnesses

A phantom tag can serve as a *witness* — a compile-time proof that a property holds:

```cpp
struct Unvalidated {};
struct Validated {};

template<typename State>
struct EmailAddress {
    std::string value;
};

auto validate(EmailAddress<Unvalidated> raw) -> std::optional<EmailAddress<Validated>> {
    if (raw.value.find('@') != std::string::npos)
        return EmailAddress<Validated>{raw.value};
    return std::nullopt;
}

void send(EmailAddress<Validated> to);  // only accepts validated emails
```

Through the Curry-Howard correspondence ([part 1](/post/type-theoretic-foundations)):

- `EmailAddress<Validated>` is a *proposition*: "this email address has been validated"
- A value of type `EmailAddress<Validated>` is a *proof* of that proposition
- The `validate` function is the *proof procedure*: it either produces evidence or fails
- The `send` function requires the proof as a precondition

The proof is erased at runtime — zero cost. But the type checker propagates it. You cannot construct an `EmailAddress<Validated>` without going through `validate`. And once you have one, every function downstream can trust it without re-checking.

This is a lightweight form of **refinement types** — types that carry proofs of properties. Full refinement types (as in Liquid Haskell or F*) can express arbitrary predicates. Phantom types give you a restricted but practical subset: boolean properties (validated/not, encrypted/not, sanitized/not) at zero cost.

## Units of Measurement

Phantom types shine in dimensional analysis:

```cpp
struct Meters {};
struct Feet {};
struct Seconds {};

template<typename Unit>
struct Quantity {
    double value;

    auto operator+(Quantity other) const -> Quantity { return {value + other.value}; }
    auto operator-(Quantity other) const -> Quantity { return {value - other.value}; }
    auto operator*(double scalar) const -> Quantity { return {value * scalar}; }
};

auto distance = Quantity<Meters>{100.0};
auto duration = Quantity<Seconds>{9.58};

// This does not compile:
// auto nonsense = distance + duration;  // Meters + Seconds = type error

auto to_feet(Quantity<Meters> m) -> Quantity<Feet> {
    return {m.value * 3.28084};
}
```

The phantom types encode the unit. Addition of same-unit quantities works. Cross-unit addition is a type error. Unit conversion is explicit.

The [$125 million Mars Climate Orbiter](https://en.wikipedia.org/wiki/Mars_Climate_Orbiter) was lost because Lockheed Martin used pound-force seconds while NASA expected newton seconds. Phantom types make this class of bug a compile error.

## The Newtype Pattern Formally

In type theory, a newtype is an **isomorphism** `A ≅ B` witnessed by explicit wrap/unwrap functions:

```
wrap   : Underlying → NewType
unwrap : NewType → Underlying
unwrap(wrap(x)) = x  ∀x
wrap(unwrap(y)) = y  ∀y
```

The key property: the isomorphism is *not implicit*. You must explicitly wrap and unwrap. This friction is the feature — it prevents accidental mixing while allowing deliberate conversion.

In Haskell, `newtype` is a language keyword that guarantees zero-cost wrapping. In Rust, tuple structs (`struct Meters(f64)`) serve the same purpose. In C++, we build it with templates and phantom tags. The mechanism differs but the type-theoretic idea is the same: **semantic distinctions encoded as type distinctions, at zero runtime cost**.

The isomorphism property matters: you must be able to get the underlying value back. `Strong<Tag, T>` exposes `.value`, completing the isomorphism. A phantom-typed wrapper that hid the underlying value entirely would be an *abstract type* (even stronger than a newtype), which is sometimes what you want but is a different pattern.

## Zero-Cost Proof

Let us verify the claim:

```cpp
auto add_ids(Strong<PassengerTag> a, Strong<PassengerTag> b) -> Strong<PassengerTag> {
    return Strong<PassengerTag>{a.value + b.value};
}

auto add_ints(int a, int b) -> int {
    return a + b;
}
```

At `-O2`, both compile to identical x86:

```asm
add_ids:
    lea eax, [rdi + rsi]
    ret

add_ints:
    lea eax, [rdi + rsi]
    ret
```

The phantom tag, the struct wrapper, the explicit constructor — all gone. The compiler sees through the abstraction completely. You pay nothing at runtime for compile-time type safety.

## Phantom Types in Loom

Loom uses phantom-tagged strong types for every domain string. The `StrongType` template in `include/loom/core/strong_type.hpp`:

```cpp
template<typename T, typename Tag>
class StrongType {
    T value_;
public:
    explicit StrongType(T value) : value_(std::move(value)) {}

    T get() const { return value_; }

    bool empty() const requires requires(const T& v) { v.empty(); }
    { return value_.empty(); }

    bool operator==(const StrongType& other) const { return value_ == other.value_; }
    bool operator!=(const StrongType& other) const { return value_ != other.value_; }
    bool operator<(const StrongType& other) const { return value_ < other.value_; }
};
```

The `Tag` parameter is the phantom. It occupies zero bytes, appears nowhere in the object layout, and vanishes completely in the binary. The `explicit` constructor prevents implicit conversion — you must deliberately introduce a value into the type.

The full set of domain types in `include/loom/core/types.hpp`:

```cpp
struct SlugTag {};
struct TitleTag {};
struct PostIdTag {};
struct TagTag {};
struct ContentTag {};
struct SeriesTag {};

using Slug    = StrongType<std::string, SlugTag>;
using Title   = StrongType<std::string, TitleTag>;
using PostId  = StrongType<std::string, PostIdTag>;
using Tag     = StrongType<std::string, TagTag>;
using Content = StrongType<std::string, ContentTag>;
using Series  = StrongType<std::string, SeriesTag>;
```

Six types. All `std::string` underneath. All distinct in the type system.

### Bugs This Prevents

Consider a function that builds a URL for a post:

```cpp
auto post_url(Slug slug) -> std::string {
    return "/post/" + slug.get();
}
```

If a caller accidentally passes a `Title` — which is also a string and might even look like a slug — the compiler rejects it. Without phantom types, this is a silent bug: the URL works for some titles, breaks for titles with spaces or special characters, and the error surfaces only at runtime.

The same protection applies throughout the domain model. A `Post` struct holds `Slug`, `Title`, `PostId`, `Content`, `Series` — each a `std::string` internally, each a distinct type externally. The struct fields cannot be swapped accidentally because the types forbid it.

The `requires` clause on `empty()` is a conditional formation rule: the method exists only when the underlying type supports it. For `StrongType<std::string, SlugTag>`, `empty()` is available because `std::string` has `empty()`. For a hypothetical `StrongType<int, CountTag>`, it would not be — the formation rule would not be satisfied.

## When Not to Use Phantom Types

**When the types need genuinely different data.** If a `Circle` has a radius and a `Rectangle` has width and height, they are structurally different types. Use a sum type (variant), not phantom tags on a shared struct.

**When the distinction is transient.** If you need to distinguish "sorted" from "unsorted" within a single function scope, a comment suffices. Phantom types pay off when the distinction crosses function boundaries.

**When you need runtime discrimination.** A phantom type cannot be inspected at runtime. If you need `if (is_validated)`, use a variant or enum. The phantom tag is not stored — it exists only in the type checker's reasoning.

## Closing: What the Compiler Cannot See, It Cannot Protect

Phantom types are how you tell the compiler about distinctions it cannot infer on its own. They are cheap (zero cost), easy (one-line type aliases), and preventive (they catch errors at the point of misuse, not at the point of failure).

The theoretical foundation is parametricity: code that is generic in the phantom tag cannot observe or violate it. The practical consequence is representation independence: the runtime behavior depends only on the underlying data, not on the tag. The type-theoretic framing is proof witnesses: a phantom tag is a compile-time proof that a property holds.

The general principle: **if two things should not be interchangeable, they should not have the same type.** Phantom types make that possible even when the underlying representation is identical.

In the [next post](/post/typestate-programming), we take this further. Instead of tagging values with *what they are*, we tag them with *what has happened to them* — encoding entire state machines in the type system, grounded in the theory of linear logic and substructural types.
