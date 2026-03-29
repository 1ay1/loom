---
title: "Sum Types and Product Types — The Algebra of C++ Types"
date: 2026-04-05
slug: algebraic-data-types
tags: [c++20, type-theory, variant, tuple, algebraic-types]
excerpt: "Every type is an algebraic expression. Product types multiply possibilities, sum types add them, and understanding this algebra transforms how you model domains in C++."
---

In the [previous post](/post/type-theoretic-foundations) we established a principle: types are not just containers for data — they encode truths about a domain. Now we need a vocabulary for combining types. That vocabulary is algebra.

Not the algebra of numbers. The algebra of *types*. Every type you write in C++ — every struct, every enum, every variant — is an algebraic expression. Product types multiply the space of possible values. Sum types add them. Understanding this algebra does not just make your thinking cleaner. It changes the types you write and the bugs you prevent.

This post builds on [post #11 on std::variant](/post/variant-visit-and-sum-types) from the C++ series. If you are comfortable with `std::variant` and `std::visit`, you have the tools. This post gives you the theory behind them.

## The Arithmetic of Types

Every type has a *cardinality* — the number of distinct values it can hold. `bool` has cardinality 2. An `enum` with four enumerators has cardinality 4. A `uint8_t` has cardinality 256.

This cardinality follows arithmetic rules, and the rules are not metaphorical. They are exact.

### Product Types: Multiplication

A `struct` combines its fields. A value of the struct must provide a value for *every* field. The total number of possible values is the product of the field cardinalities:

```cpp
struct Pixel {
    uint8_t r;  // 256 values
    uint8_t g;  // 256 values
    uint8_t b;  // 256 values
};
// Cardinality: 256 × 256 × 256 = 16,777,216
```

This is why structs are called **product types**. A `std::pair<A, B>` has cardinality |A| × |B|. A `std::tuple<A, B, C>` has cardinality |A| × |B| × |C|. The pattern is multiplication.

This explains something practical: **every field you add to a struct multiplies the state space.** Adding a `bool` to a struct doubles the number of possible values. Adding an `enum` with five variants quintuples it. If your struct has fields that are independent — that can vary freely without constraint — then the state space grows combinatorially, and most of those combinations may be meaningless.

### Sum Types: Addition

A `std::variant` holds *one of* its alternatives. A value of the variant is a value of exactly one alternative:

```cpp
using Color = std::variant<
    RGB,        // 16,777,216 values
    Grayscale,  // 256 values
    Transparent // 1 value (empty struct)
>;
// Cardinality: 16,777,216 + 256 + 1 = 16,777,473
```

This is why variants are called **sum types**. The total cardinality is the *sum* of the alternative cardinalities. A value is one thing *or* another — never both.

### The Unit Type and the Void Type

Two special types complete the algebra:

**The unit type** has exactly one value. In C++ this is an empty struct (`struct Unit {}`) or `std::monostate`. Its cardinality is 1. It is the multiplicative identity: `A × 1 ≅ A`. A struct with a unit-type field is isomorphic to the struct without it — the field carries no information.

**The void type** has zero values — it is uninhabitable. In type theory it is called the *bottom type* (⊥). A function returning `void` in C++ is not quite the same thing (it returns, just with no value), but `std::variant<>` with no alternatives would be the true void — a type no value can ever inhabit. It is the additive identity: `A + 0 ≅ A`.

### The Algebra in Practice

Once you see the arithmetic, type design becomes equation solving.

**Problem:** model a configuration value that is either an integer, a string, or absent.

```cpp
// Algebraic expression: int + string + 1
using ConfigValue = std::variant<int, std::string, std::monostate>;
```

**Problem:** model an HTTP response that always has a status code and optionally has a body.

```cpp
// Algebraic expression: int × (string + 1)
struct HttpResponse {
    int status;                        // always present
    std::optional<std::string> body;   // present or absent
};
```

`std::optional<T>` is `T + 1` — the type T plus one additional state (empty). This is not a coincidence. `optional` *is* a sum type with two alternatives.

## Why Enums Are Weak Sum Types

C++ enums look like sum types:

```cpp
enum class Shape { Circle, Rectangle, Triangle };
```

But they are *degenerate* sums — each alternative carries no data. A `Shape` tells you *which* shape, but not its dimensions. To associate data with each case, the pre-variant approach was:

```cpp
struct ShapeData {
    Shape kind;
    double radius;    // only valid if Circle
    double width;     // only valid if Rectangle
    double height;    // only valid if Rectangle or Triangle
    double base;      // only valid if Triangle
};
```

This is a product pretending to be a sum. It allocates space for all fields and uses the `kind` tag to determine which ones are valid. Every function must check the tag before accessing a field. Accessing `radius` on a rectangle is undefined — not by the type system, but by convention.

The variant version:

```cpp
struct Circle { double radius; };
struct Rectangle { double width, height; };
struct Triangle { double base, height; };

using Shape = std::variant<Circle, Rectangle, Triangle>;
```

Each alternative carries exactly the data it needs. There is no `radius` field on a rectangle because `Rectangle` does not have one. The compiler enforces this. And `std::visit` gives you exhaustive matching — you must handle every case:

```cpp
auto area(const Shape& s) -> double {
    return std::visit(overloaded{
        [](const Circle& c)    { return std::numbers::pi * c.radius * c.radius; },
        [](const Rectangle& r) { return r.width * r.height; },
        [](const Triangle& t)  { return 0.5 * t.base * t.height; },
    }, s);
}
```

If you add a `Pentagon` to the variant and forget to handle it here, the compiler tells you. With the enum+struct approach, it silently does the wrong thing.

## The Overloaded Pattern

The `overloaded` helper used above is a small but essential pattern for working with sum types:

```cpp
template<typename... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
```

One line. It inherits from all the lambdas and brings their `operator()` into scope. Combined with C++17 class template argument deduction, this lets you write inline visitors as a list of lambdas. This was covered in [post #8 on fold expressions](/post/variadic-templates-and-fold-expressions) — the `using Ts::operator()...` is a pack expansion.

## Products with Invariants

A naked product type — a struct with public fields — makes no promises. Any combination of field values is representable:

```cpp
struct DateRange {
    Date start;
    Date end;
};
```

This permits `start > end`, which is nonsensical. The product is too large — it includes values that violate a domain invariant.

The fix is to **restrict construction**:

```cpp
class DateRange {
    Date start_, end_;
public:
    static auto make(Date start, Date end) -> std::optional<DateRange> {
        if (start > end) return std::nullopt;
        return DateRange{start, end};
    }
    auto start() const -> Date { return start_; }
    auto end() const -> Date { return end_; }
private:
    DateRange(Date s, Date e) : start_(s), end_(e) {}
};
```

The product type `Date × Date` has too many inhabitants. `DateRange` is a *refinement* — a subset of the product where the invariant `start ≤ end` holds. The private constructor ensures the invariant is established at creation. The public accessors ensure it is never violated afterward.

This pattern — product type + private constructor + factory function — is the standard way to shrink a product's cardinality to match the domain.

## Sums for Error Handling

The standard error handling pattern `std::optional<T>` is a sum type: `T + 1`. It encodes "a T or nothing." But sometimes "nothing" is not enough — you need to know *why* something failed.

```cpp
// T + E — a value or an error
template<typename T, typename E>
using Result = std::variant<T, E>;

struct ParseError { std::string message; size_t position; };

auto parse_int(std::string_view input) -> Result<int, ParseError> {
    // ...
    if (failed)
        return ParseError{"expected digit", pos};
    return value;
}
```

Now the caller must handle both cases — `std::visit` enforces exhaustiveness. Compare this to the traditional approach of returning a sentinel value (`-1`) or throwing an exception. The sentinel is a lie — it pretends to be a valid `int` but carries no error information. The exception is invisible in the type signature. The variant makes the duality explicit and forces the caller to confront it.

C++23's `std::expected<T, E>` formalizes this pattern with nicer syntax, but the algebraic structure is the same: `T + E`.

## When to Use Products vs Sums

A useful heuristic:

**Use a product** when multiple pieces of information are independently meaningful and always present together. A point has both an x and a y. An HTTP request has both a method and a path. These are genuinely independent dimensions.

**Use a sum** when a value can be one of several *mutually exclusive* alternatives. A JSON value is a number *or* a string *or* an array *or* an object *or* null — never more than one. A network result is a response *or* an error.

**The dangerous middle ground** is a product with a tag field, where different fields are "valid" depending on the tag. This is a sum implemented as a product, and it permits contradictions. If you find yourself writing `if (kind == X) { use field_a; } else { use field_b; }`, you probably need a variant.

## Recursive Types and Expression Trees

Sum types become especially powerful when they are recursive — when an alternative contains the sum type itself:

```cpp
struct Literal { double value; };
struct Negate;
struct Add;
struct Multiply;

using Expr = std::variant<
    Literal,
    std::unique_ptr<Negate>,
    std::unique_ptr<Add>,
    std::unique_ptr<Multiply>
>;

struct Negate { Expr operand; };
struct Add { Expr left, right; };
struct Multiply { Expr left, right; };
```

This is an algebraic data type in the full sense — a recursively defined sum of products. An expression is either a literal, a negation (which contains an expression), an addition (two expressions), or a multiplication (two expressions). The recursion must go through a pointer (here `unique_ptr`) because C++ needs to know the size of the variant, and a directly recursive variant would have infinite size.

Evaluating this tree is a recursive visit:

```cpp
auto eval(const Expr& e) -> double {
    return std::visit(overloaded{
        [](const Literal& lit) { return lit.value; },
        [](const std::unique_ptr<Negate>& n) { return -eval(n->operand); },
        [](const std::unique_ptr<Add>& a) { return eval(a->left) + eval(a->right); },
        [](const std::unique_ptr<Multiply>& m) { return eval(m->left) * eval(m->right); },
    }, e);
}
```

No dynamic dispatch. No inheritance hierarchy. No virtual methods. Just types and pattern matching.

## The Algebra of Loom

Loom's domain model is algebraic throughout. The `Node` type in the [DOM DSL](/post/dom-and-components) is a sum:

```cpp
enum class NodeKind { Element, Void, Text, Raw, Fragment };
```

A node is *one of* five kinds — element, void element, text, raw HTML, or fragment. Each kind carries different data. This is the same pattern as the `Shape` example, scaled to a real system.

The `ThemeDef` in the [theme system](/post/theme-system) is a product: colors × typography × shape × density × component styles. Each field is independently configurable. The combination of all fields defines a complete theme.

And `CachedPage` in the [cache](/post/cache-and-rendering) is a product of six pre-serialized HTTP variants: gzip+keepalive × gzip+close × plain+keepalive × plain+close × 304+keepalive × 304+close. The server selects one variant per request — choosing from a sum (which format to use) indexed into a product (all formats pre-built).

Once you see the algebra, you see it everywhere.

## Closing: Types Have Arithmetic

The next time you define a struct, count its inhabitants. Multiply the cardinalities of its fields. If that number is vastly larger than the set of values that actually make sense in your domain, your type is too permissive. Some of those extra inhabitants are bugs waiting to happen.

The fix is either to restrict construction (private constructors, factory functions) or to restructure the type (sums instead of products-with-tags). The algebra tells you which approach to use.

In the [next post](/post/phantom-types), we will see how to add type-level distinctions that carry *zero data* — phantom types that make the compiler see differences that do not exist at runtime.
