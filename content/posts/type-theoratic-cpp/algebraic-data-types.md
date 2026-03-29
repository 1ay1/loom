---
title: "Sum Types and Product Types — The Algebra of C++ Types"
date: 2026-03-21
slug: algebraic-data-types
tags: [c++20, type-theory, variant, tuple, algebraic-types, initial-algebras]
excerpt: "Types form a semiring. Products multiply, sums add, and the distributive law lets you factor types like polynomials. Initial algebras, catamorphisms, and the deep reason why std::visit is the only operation you need."
---

In the [previous post](/post/type-theoretic-foundations) we established a principle: types are not just containers for data — they encode truths about a domain. We introduced formation, introduction, and elimination rules. Now we need a vocabulary for *combining* types. That vocabulary is algebra — the literal algebra of types, where the arithmetic of natural numbers shows up in the structure of your data.

This is not a metaphor. Types form a mathematical structure called a **semiring**, with the same laws as natural number arithmetic. Understanding this algebra changes how you design types, how you spot redundancy, and how you recognize the fundamental operations on data.

This post builds on [post #11 on std::variant](/post/variant-visit-and-sum-types) from the C++ series. If you are comfortable with `std::variant` and `std::visit`, you have the tools. This post gives you the theory behind them.

## The Arithmetic of Types

Every type has a *cardinality* — the number of distinct values it can hold. `bool` has cardinality 2. An `enum` with four enumerators has cardinality 4. A `uint8_t` has cardinality 256.

This cardinality follows arithmetic rules, and the rules are exact.

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

This is why structs are called **product types**. `std::pair<A, B>` has cardinality |A| × |B|. `std::tuple<A, B, C>` has cardinality |A| × |B| × |C|.

Every field you add to a struct *multiplies* the state space. Adding a `bool` doubles it. Adding a five-variant enum quintuples it. If most combinations are meaningless, your type is too permissive.

### Sum Types: Addition

A `std::variant` holds *one of* its alternatives:

```cpp
using Color = std::variant<
    RGB,        // 16,777,216 values
    Grayscale,  // 256 values
    Transparent // 1 value (empty struct)
>;
// Cardinality: 16,777,216 + 256 + 1 = 16,777,473
```

Variants are **sum types**. The cardinality is the *sum* of the alternatives.

### The Unit and the Void

**The unit type** has exactly one value: `std::monostate` or an empty struct. Cardinality 1. It is the multiplicative identity: `A × 1 ≅ A`.

**The void type** has zero values — uninhabitable. `std::variant<>` (no alternatives) would be the true void. Cardinality 0. It is the additive identity: `A + 0 ≅ A`.

## The Semiring of Types

Here is where it gets remarkable. Types under sum (+) and product (×) satisfy the same laws as natural numbers under addition and multiplication. This structure is called a **semiring**:

### Additive identity
```
A + 0 ≅ A
```
A variant with an uninhabitable alternative is isomorphic to a variant without it. Adding "nothing" changes nothing.

### Multiplicative identity
```
A × 1 ≅ A
```
A struct with a unit field is isomorphic to the struct without it. A field carrying no information can be dropped.

### Absorbing element
```
A × 0 ≅ 0
```
A struct with an uninhabitable field is itself uninhabitable. If one component can never be provided, the whole product can never be constructed. In C++: if a struct has a field of an uninhabitable type, you can never create an instance.

### Commutativity
```
A + B ≅ B + A
A × B ≅ B × A
```
`variant<int, string>` and `variant<string, int>` are isomorphic — they hold the same set of values, just in different order. Similarly for pairs.

### Associativity
```
(A + B) + C ≅ A + (B + C)
(A × B) × C ≅ A × (B × C)
```
Nesting does not change the value space. `pair<pair<A,B>, C>` and `tuple<A, B, C>` are isomorphic.

### The Distributive Law

This is the powerful one:

```
A × (B + C) ≅ (A × B) + (A × C)
```

In C++: a struct containing a variant is isomorphic to a variant of structs. Let us see this concretely:

```cpp
// A × (B + C): a struct containing a variant
struct Request {
    UserId user;                              // A
    std::variant<GetParams, PostBody> payload; // B + C
};

// (A × B) + (A × C): a variant of structs
using Request2 = std::variant<
    GetRequest,   // { UserId user; GetParams params; }    — A × B
    PostRequest   // { UserId user; PostBody body; }       — A × C
>;
```

These two representations are *isomorphic* — they hold exactly the same information. But they have different ergonomics. The variant-of-structs version eliminates the possibility of accessing `PostBody` on a GET request because each variant alternative carries only its own relevant data. The distributive law lets you *refactor* between them.

This is type-level factoring. Just as `3 × (5 + 7) = 3 × 5 + 3 × 7` in arithmetic, you can factor or distribute type expressions. When you spot a product that contains a sum, consider whether distributing gives you a cleaner type.

## `std::optional<T>` Is `T + 1`

`std::optional<T>` is a sum type with two alternatives: a T or nothing. "Nothing" is the unit type (one value: the empty state). So `optional<T>` has cardinality |T| + 1.

This means `optional` obeys the algebra:

```
optional<optional<T>> ≅ T + 1 + 1 ≅ T + 2
```

A nested optional has three states: empty-outer, empty-inner, or a T. But `optional<optional<T>>` distinguishes `nullopt` (empty outer) from `optional<T>{nullopt}` (non-empty outer containing empty inner). This is algebraically correct: `(T + 1) + 1 = T + 2`, and the two `+1`s are distinct.

## Why Enums Are Weak Sum Types

C++ enums look like sum types but are *degenerate* — each alternative carries no data:

```cpp
enum class Shape { Circle, Rectangle, Triangle };  // 1 + 1 + 1 = 3
```

To associate data with each case, the pre-variant approach was a product-pretending-to-be-a-sum:

```cpp
struct ShapeData {
    Shape kind;
    double radius;    // only valid if Circle
    double width;     // only valid if Rectangle
    double height;    // only valid if Rectangle or Triangle
};
```

This has cardinality `3 × ℝ × ℝ × ℝ` — massively larger than the domain requires. Most inhabitants are contradictions. The variant version:

```cpp
struct Circle { double radius; };
struct Rectangle { double width, height; };
struct Triangle { double base, height; };

using Shape = std::variant<Circle, Rectangle, Triangle>;
```

Cardinality: `ℝ + ℝ² + ℝ²` — exactly the right size. No dead fields, no invalid combinations.

## The Overloaded Pattern

The essential tool for working with sum types:

```cpp
template<typename... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
```

One line. It inherits from all the lambdas and brings their `operator()` into scope. Combined with class template argument deduction:

```cpp
auto area(const Shape& s) -> double {
    return std::visit(overloaded{
        [](const Circle& c)    { return std::numbers::pi * c.radius * c.radius; },
        [](const Rectangle& r) { return r.width * r.height; },
        [](const Triangle& t)  { return 0.5 * t.base * t.height; },
    }, s);
}
```

If you add a `Pentagon` and forget to handle it, the compiler rejects the program. The elimination rule is enforced.

## Initial Algebras: Why ADTs Are Special

Now for the deeper theory. Why are algebraic data types so natural? Why does `std::visit` feel like *the* fundamental operation? The answer comes from a concept called **initial algebras**.

An **algebra** for a type constructor F is a type A together with a function `F(A) → A`. Think of F as the "shape" of one layer of your data, and the algebra as a way to "collapse" one layer into a result.

For a list of integers, the shape functor is `F(X) = 1 + int × X` — either empty (1) or an element and a tail (int × X). An algebra for this functor is any type A with a function `(1 + int × A) → A`. Unpacking, this means: a way to handle the empty case (a value of type A) and a way to combine an int with an A to get another A.

Here is the remarkable fact: the list type *itself* is the **initial algebra** — the "simplest" algebra for this functor, from which there is a unique function (called a **catamorphism**) to any other algebra.

What does that mean in practice? It means:

```cpp
// Sum: the algebra is (0, λ(x, acc). x + acc)
auto sum = std::accumulate(vec.begin(), vec.end(), 0);

// Length: the algebra is (0, λ(_, acc). 1 + acc)
auto len = std::accumulate(vec.begin(), vec.end(), 0,
    [](int acc, int) { return acc + 1; });

// To string: the algebra is ("", λ(x, acc). acc + to_string(x))
auto str = std::accumulate(vec.begin(), vec.end(), std::string{},
    [](std::string acc, int x) { return acc + std::to_string(x) + ","; });
```

Every `std::accumulate` call is a catamorphism — the unique homomorphism from the initial algebra (the list) to your target algebra (the result type with your combining function). Different algebras = different folds. The *structure* is always the same: peel off one layer, apply the algebra, recurse.

The word "catamorphism" comes from the Greek *kata* (downward) — you are tearing down a structure level by level. `std::visit` on a non-recursive variant is a catamorphism for the degenerate case of a one-layer-deep structure.

## Polynomial Functors: Types as Equations

The shape of an ADT can be written as a polynomial. This is not a loose analogy — it is the actual mathematical structure:

```
List<A>  = μX. 1 + A × X          (empty or cons)
Tree<A>  = μX. A + X × X          (leaf or branch)
Nat      = μX. 1 + X              (zero or successor)
Maybe<A> = 1 + A                   (nothing or just)
Either<A,B> = A + B                (left or right)
```

The `μX` is the "fixed point" operator — it ties the recursive knot. `List<A>` is the smallest type X satisfying `X = 1 + A × X`.

Now watch this: if we treat the equation algebraically and "solve" for X:

```
X = 1 + A × X
X - A × X = 1
X × (1 - A) = 1
X = 1 / (1 - A)
```

And `1 / (1 - A)` is the geometric series: `1 + A + A² + A³ + ...`

A list of A is: either empty (1), or one element (A), or two elements (A²), or three elements (A³), and so on. The algebraic solution *is* the type. The formal power series expansion gives you the cardinality of each "layer" of the recursive type.

This is not numerology. It is a real correspondence between combinatorics and type theory, studied by Joyal, Fiore, and others under the name **combinatorial species**. The algebra of types is isomorphic to the algebra of formal power series.

## Products with Invariants

A naked product with public fields makes no promises:

```cpp
struct DateRange {
    Date start;
    Date end;  // could be before start!
};
```

This permits `start > end` — a contradiction in the domain. The product is too large. The fix is to **restrict construction**:

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

In type-theoretic terms: `DateRange` is a **refinement type** — a subset of `Date × Date` where the invariant `start ≤ end` holds. The private constructor restricts the introduction rule to only admit valid inhabitants.

## Sums for Error Handling

`std::optional<T>` is `T + 1` — a value or nothing. But sometimes "nothing" is insufficient. You need to know *why*:

```cpp
template<typename T, typename E>
using Result = std::variant<T, E>;

struct ParseError { std::string message; size_t position; };

auto parse_int(std::string_view input) -> Result<int, ParseError> {
    // ...
    if (failed) return ParseError{"expected digit", pos};
    return value;
}
```

The caller must handle both cases — `std::visit` enforces exhaustiveness. This is `T + E`, the sum of success and failure. Compare to returning `-1` (a lie inside the type) or throwing (invisible in the signature). The variant makes the disjunction explicit.

C++23's `std::expected<T, E>` formalizes this pattern with nicer syntax, but the algebraic structure is identical: `T + E`.

## The Expression Problem

There is a deep tension in how you organize data, and it has type-theoretic roots.

**Sum types** (variants/ADTs) make adding new *operations* easy — just write a new `std::visit` with a new set of cases. But adding a new *variant* is hard: every existing visitor must be updated.

**Subtype polymorphism** (virtual functions/OOP) makes adding new *variants* easy — just write a new subclass. But adding a new *operation* is hard: every existing class must be updated with a new virtual method.

This is the **expression problem**, named by Philip Wadler. It is not a practical annoyance — it is a fundamental duality in type theory. Sum types give you *closed* data (fixed set of alternatives) with *open* operations. Class hierarchies give you *open* data (new subclasses anytime) with *closed* operations (fixed set of virtual methods).

Neither is universally better. The right choice depends on which axis of extension matters more in your domain. In a compiler, expression types are fixed but operations (optimization passes, code generators) multiply — use ADTs. In a GUI framework, operations are fixed (draw, resize, focus) but widget types multiply — use classes.

## Isomorphisms and Canonical Forms

Two types with the same cardinality are **isomorphic** — there exists a pair of functions converting between them without losing information.

```
std::pair<bool, A> ≅ std::variant<A, A>
```

Because `2 × |A| = |A| + |A|`. A pair of a bool and an A is the same information as choosing between two copies of A. You can convert freely:

```cpp
// pair<bool, A> → variant<A, A>
template<typename A>
auto to_variant(std::pair<bool, A> p) -> std::variant<A, A> {
    if (p.first) return std::variant<A, A>(std::in_place_index<0>, std::move(p.second));
    else return std::variant<A, A>(std::in_place_index<1>, std::move(p.second));
}
```

When you spot an isomorphism, you can choose whichever representation is more ergonomic. The algebra tells you they are equivalent.

## Recursive Types in C++

Recursive variants need indirection because C++ must know sizes at compile time:

```cpp
struct Literal { double value; };
struct Negate;
struct Add;

using Expr = std::variant<
    Literal,
    std::unique_ptr<Negate>,
    std::unique_ptr<Add>
>;

struct Negate { Expr operand; };
struct Add { Expr left, right; };
```

Evaluating is a recursive catamorphism:

```cpp
auto eval(const Expr& e) -> double {
    return std::visit(overloaded{
        [](const Literal& lit) { return lit.value; },
        [](const std::unique_ptr<Negate>& n) { return -eval(n->operand); },
        [](const std::unique_ptr<Add>& a) { return eval(a->left) + eval(a->right); },
    }, e);
}
```

No dynamic dispatch. No inheritance hierarchy. Just types and catamorphisms. We will explore the theory of recursive types much more deeply in [part 9](/post/recursive-types-and-fixed-points).

## Closing: Types Have Arithmetic

The next time you define a struct, count its inhabitants. Multiply the cardinalities of its fields. If that number is vastly larger than the set of values that actually make sense, your type is too permissive. Some of those extra inhabitants are bugs waiting to happen.

The fix is either to restrict construction (private constructors, factory functions — tightening the introduction rule) or to restructure the type (sums instead of products-with-tags — changing the algebraic expression). The semiring laws tell you which transformations are valid. The distributive law tells you when to factor. The initial algebra tells you that folding is the fundamental way to consume data.

In the [next post](/post/phantom-types), we will see how to add type-level distinctions that carry *zero data* — phantom types that leverage parametricity to make the compiler see differences that do not exist at runtime.
