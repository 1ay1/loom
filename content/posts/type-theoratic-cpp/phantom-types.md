---
title: "Phantom Types — Making the Compiler See What Isn't There"
date: 2026-04-12
slug: phantom-types
tags: [c++20, type-theory, phantom-types, templates, zero-cost]
excerpt: "A phantom type parameter exists only in the type system — it carries no data, occupies no memory, and vanishes in the binary. But it can prevent entire classes of bugs."
---

In the [last post](/post/algebraic-data-types), we saw how product and sum types shape the space of possible values. But sometimes the distinction you need to enforce is not between different *data* — it is between different *meanings* of the same data.

A user ID and an order ID are both integers. A meters value and a feet value are both doubles. A validated string and an unvalidated string are both `std::string`. Structurally identical. Semantically incompatible.

We need the compiler to see a difference that does not exist in the data. The technique is called **phantom types** — type parameters that appear in the type signature but are never stored, never accessed, and never present at runtime. They are ghosts in the type system. And they are one of the most powerful tools in type-theoretic C++.

This post builds on [post #7 on templates](/post/templates-and-generic-programming) and [post #10 on concepts](/post/concepts-and-constraints) from the C++ series, and directly connects to the `StrongType` pattern described in the [strong types internals post](/post/strong-types).

## The Problem: Structural Equality

The C++ type system is *structural* by default for built-in types. Two `int`s are the same type, regardless of what they represent:

```cpp
void book_flight(int passenger_id, int flight_id, int seat_number);
```

The compiler sees three `int` parameters. It cannot tell them apart. If you pass the arguments in the wrong order, the code compiles, runs, and silently does the wrong thing. The bug is not in your logic — it is in your *types*. The type `int` is too broad. It admits values from unrelated domains and lets them mingle freely.

## The Phantom: A Tag That Vanishes

The solution is a wrapper parameterized by a *tag type* that exists only at compile time:

```cpp
template<typename Tag, typename T = int>
struct Strong {
    T value;

    explicit Strong(T v) : value(v) {}

    auto operator==(const Strong& other) const -> bool { return value == other.value; }
    auto operator<=>(const Strong& other) const = default;
};
```

The `Tag` parameter is the phantom. It appears in the type `Strong<Tag, T>` but is never stored as a field, never accessed in any method, never present in the object's memory layout. `sizeof(Strong<Tag, T>) == sizeof(T)` — the tag is purely a compile-time artifact.

Now we define distinct types:

```cpp
struct PassengerTag {};
struct FlightTag {};
struct SeatTag {};

using PassengerId = Strong<PassengerTag>;
using FlightId = Strong<FlightTag>;
using SeatNumber = Strong<SeatTag>;

void book_flight(PassengerId passenger, FlightId flight, SeatNumber seat);
```

Each ID type wraps an `int`. All three have the same size, the same memory layout, the same runtime representation. But the compiler treats them as completely different types. Passing a `FlightId` where a `PassengerId` is expected is a compile error. Passing a bare `int` is also an error — the `explicit` constructor requires conscious conversion.

The phantom tag is the key. It contributes zero bytes to the struct but creates an entirely distinct type. This is the essence of phantom typing: **information that exists only in the type system, invisible at runtime, but enforced at every usage site.**

## Why Not Just Use Separate Structs?

You could define each type independently:

```cpp
struct PassengerId { int value; };
struct FlightId { int value; };
struct SeatNumber { int value; };
```

This works and is sometimes the right choice. But the template approach has advantages:

**Shared behavior.** If all your ID types need comparison, hashing, serialization, and formatting, you write those once in the template:

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

// Hashing
template<typename Tag, typename T>
struct std::hash<Strong<Tag, T>> {
    auto operator()(const Strong<Tag, T>& s) const -> size_t {
        return std::hash<T>{}(s.value);
    }
};
```

Every strong type gets comparison, ordering, streaming, and hashing for free. No code duplication. If you add a new ID type, it is one line:

```cpp
using InvoiceId = Strong<struct InvoiceTag>;
```

The `struct InvoiceTag` is defined inline — a forward declaration used only as a phantom parameter. It never needs a body.

**Conditional interfaces.** With concepts, the template can expose different operations depending on the underlying type:

```cpp
template<typename Tag, typename T>
struct Strong {
    T value;

    // Arithmetic only when T supports it
    auto operator+(const Strong& other) const -> Strong
        requires requires(T a, T b) { a + b; }
    {
        return Strong{value + other.value};
    }
};
```

ID types (tagged `int`) do not get arithmetic — you cannot "add" two user IDs. Measurement types (tagged `double`) do. Same template, different capabilities, driven by the tag and the underlying type.

## Phantom Types for State

Phantoms are not limited to distinguishing values. They can encode *states*. This bridges to typestate programming (covered fully in [part 4](/post/typestate-programming)), but the mechanism is worth seeing here.

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

`EmailAddress<Unvalidated>` and `EmailAddress<Validated>` are different types. They have the same layout (a single `std::string`), but you cannot pass one where the other is expected. The phantom parameter is a compile-time proof of validation state.

The `send` function does not need to check validity. The type proves it. The validation boundary is the `validate` function, and the proof propagates through the type system from that point forward.

## Units of Measurement

Phantom types shine in dimensional analysis. Mixing meters and feet is not a type error in most C++ code — it is a runtime surprise (and occasionally a [$125 million Mars orbiter](https://en.wikipedia.org/wiki/Mars_Climate_Orbiter)).

```cpp
struct Meters {};
struct Feet {};
struct Seconds {};
struct Kilograms {};

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

// Explicit conversion must be deliberate:
auto to_feet(Quantity<Meters> m) -> Quantity<Feet> {
    return {m.value * 3.28084};
}
```

Addition of same-unit quantities works. Addition of different-unit quantities is a compile error — the phantom types mismatch. Unit conversion is an explicit function with a clear name. You cannot accidentally mix meters and feet because the type system will not let you.

For more sophisticated dimensional analysis — where multiplying meters × meters gives square meters, or dividing meters by seconds gives velocity — you need more machinery (ratio types, template parameter packs for dimension exponents). But the core technique is the same: phantom parameters encode the unit, and the compiler enforces consistency.

## Zero-Cost Proof

Let us verify the "zero-cost" claim. Consider:

```cpp
auto add_ids(Strong<PassengerTag> a, Strong<PassengerTag> b) -> Strong<PassengerTag> {
    return Strong<PassengerTag>{a.value + b.value};
}

auto add_ints(int a, int b) -> int {
    return a + b;
}
```

At `-O2`, both functions compile to identical x86:

```asm
add_ids:
    lea eax, [rdi + rsi]
    ret

add_ints:
    lea eax, [rdi + rsi]
    ret
```

Same instruction. Same registers. The phantom tag, the struct wrapper, the explicit constructor — all gone. The compiler sees through the abstraction completely. You pay nothing at runtime for the type safety you gain at compile time.

This is not always the case with every wrapper pattern. ABI considerations can introduce overhead in some calling conventions. But for inlined code — and small structs are almost always inlined — the abstraction is free.

## Phantom Types in Loom

Loom uses phantom types extensively. The `StrongType` template from the [strong types post](/post/strong-types) is exactly this pattern:

```cpp
template<typename Tag, typename T>
struct StrongType {
    T value;
    // ...conditional methods based on T's interface...
};

using Slug    = StrongType<struct SlugTag, std::string>;
using Title   = StrongType<struct TitleTag, std::string>;
using Content = StrongType<struct ContentTag, std::string>;
using PostId  = StrongType<struct PostIdTag, std::string>;
```

Four types. All strings underneath. All distinct in the type system. A function that expects a `Slug` will never accidentally receive a `Title`, because the phantom tags differ. The tags themselves — `SlugTag`, `TitleTag` — are empty structs that exist only as type parameters and compile away to nothing.

The theme system goes further. `Color` and `FontStack` are both strings, but they are distinct phantom-tagged types:

```cpp
using Color     = StrongType<struct ColorTag, std::string>;
using FontStack = StrongType<struct FontStackTag, std::string>;
```

A color cannot be used as a font stack. A font stack cannot be used as a color. The compiler knows the difference because the phantom tags are different, even though the runtime representation is identical.

## The Newtype Pattern

The phantom type approach is an instance of a broader pattern known as **newtype** (borrowed from Haskell): creating a new, distinct type that wraps an existing type with identical runtime representation.

The essence of newtype is:

1. The wrapper adds no data beyond the wrapped value
2. The wrapper creates a distinct type that does not implicitly convert
3. The wrapper can selectively expose operations from the underlying type
4. The wrapper is erased at runtime — same size, same layout, same machine code

In Haskell, `newtype` is a language feature. In Rust, tuple structs (`struct Meters(f64)`) serve the same purpose. In C++, we build it with templates and phantom tags. The mechanism differs, but the type-theoretic idea is the same: **semantic distinctions encoded as type distinctions, at zero runtime cost**.

## When Not to Use Phantom Types

Phantoms are not always the right tool:

**When the types need genuinely different data.** If a `Circle` has a radius and a `Rectangle` has width and height, they are not the same type with different tags — they are structurally different types. Use a sum type (variant), not phantom tags on a shared struct.

**When the distinction is transient.** If you need to distinguish "sorted" from "unsorted" within a single function scope, a phantom type is overkill. A comment or a local variable name suffices. Phantom types pay off when the distinction crosses function boundaries.

**When you need runtime discrimination.** A phantom type cannot be inspected at runtime — `if (is_validated)` does not work because the tag is not stored. If you need runtime branching on the distinction, use a variant or an enum.

## Closing: What the Compiler Cannot See, It Cannot Protect

The compiler is powerful but literal. It enforces exactly the rules you give it. If two semantically different things have the same type, the compiler sees them as interchangeable. Every accidental swap, every misplaced argument, every unit mismatch sails through compilation without a warning.

Phantom types are how you tell the compiler about distinctions it cannot infer on its own. They are cheap (zero cost), easy (one-line type aliases), and preventive (they catch errors at the point of misuse, not at the point of failure).

The general principle: **if two things should not be interchangeable, they should not have the same type.** Phantom types make that possible even when the underlying representation is identical.

In the [next post](/post/typestate-programming), we take this further. Instead of tagging values with *what they are*, we tag them with *what has happened to them* — encoding entire state machines in the type system.
