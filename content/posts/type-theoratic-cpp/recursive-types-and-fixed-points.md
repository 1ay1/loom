---
title: "Recursive Types and Fixed Points — The Algebra of Data"
date: 2026-04-06
slug: recursive-types-and-fixed-points
tags: [c++20, type-theory, recursive-types, fixed-points, f-algebras, catamorphisms]
excerpt: "A list contains lists. A tree contains trees. How do you define a type in terms of itself? Fixed points, F-algebras, and catamorphisms — the formal machinery behind every fold, every visitor, and every recursive data structure."
---

A list contains... lists. A tree contains... trees. An expression contains... expressions. Recursive data structures are everywhere in programming. But how do you define a type in terms of itself without circularity?

Type theory has a precise answer: **fixed points**. A recursive type is the fixed point of a type equation — the smallest type X where X = F(X) for some functor F. Understanding this machinery reveals that `std::accumulate` is not just "a loop." It is a **catamorphism** — the unique structure-preserving map from a recursive type to any result. And `std::visit` on a recursive variant is the same thing.

This post connects the algebraic types from [part 2](/post/algebraic-data-types) to their recursive cousins and gives you the formal machinery behind every fold you have ever written.

## The Problem of Self-Reference

Consider defining a list:

```
A list of T is either:
  - empty, or
  - an element of T followed by a list of T
```

If we write this as a type equation:

```
List<T> = 1 + T × List<T>
```

The type appears on both sides. This is circular. In naive set theory, this is Russell's paradox territory. In type theory, the solution is elegant: the **least fixed point**.

## The Mu Type (μ)

The notation `μX. F(X)` means "the smallest type X such that X = F(X)." The μ (mu) is the fixed-point operator. F is a *functor* — a type-level function that describes the "shape" of one layer of the recursive structure.

The fundamental recursive types are all fixed points:

```
List<T>     = μX. 1 + T × X         // empty or cons
Tree<T>     = μX. T + X × X         // leaf or branch
Nat         = μX. 1 + X             // zero or successor
RoseTree<T> = μX. T × List<X>       // value with list of children
Expr        = μX. Lit + X + X × X   // literal, negate, add
```

Each is defined by its *shape functor* F:

- For lists: `F(X) = 1 + T × X` — one layer is "either empty or an element and a recursive part"
- For trees: `F(X) = T + X × X` — one layer is "either a leaf or two recursive parts"
- For natural numbers: `F(X) = 1 + X` — "either zero or one more than something"

The fixed point "ties the knot": it allows F to be applied infinitely many times, building up an arbitrarily deep structure.

### Natural Numbers as a Fixed Point

The natural numbers `Nat = μX. 1 + X` are perhaps the simplest recursive type. Unrolling:

```
Nat = 1 + Nat
    = 1 + (1 + Nat)
    = 1 + (1 + (1 + Nat))
    = 1 + 1 + 1 + ...
```

Zero is the `1` (the empty case). One is `1 + 1` (successor of zero). Two is `1 + 1 + 1` (successor of successor of zero). The natural numbers emerge from the fixed point of the simplest possible recursive shape.

In C++:

```cpp
struct Zero {};
struct Succ;

using Nat = std::variant<Zero, std::unique_ptr<Succ>>;

struct Succ { Nat pred; };
```

Not practical for arithmetic, but structurally exact.

## Why Pointers?

In the list definition `List<T> = 1 + T × List<T>`, the type appears inside itself. In C++, types must have known sizes at compile time. A `struct List { variant<Empty, pair<T, List>>; }` would have infinite size — each List contains another List ad infinitum.

The pointer breaks the infinite nesting by adding indirection:

```cpp
struct Empty {};

template<typename T>
struct Cons;

template<typename T>
using List = std::variant<Empty, std::unique_ptr<Cons<T>>>;

template<typename T>
struct Cons {
    T head;
    List<T> tail;
};
```

The `unique_ptr` has a fixed size (one pointer) regardless of how deep the list goes. It is the C++ incarnation of the μ operator — the mechanism that makes self-reference finite.

## F-Algebras: The Theory of Folding

Here is the key construction. An **F-algebra** for a functor F consists of:

1. A **carrier type** A
2. An **algebra map** `F(A) → A`

The idea: F describes the shape of one layer. The algebra map says "given one layer with A's in the recursive positions, produce a single A." It collapses one layer of structure.

### Example: List Algebras

For `List<int>` with shape functor `F(X) = 1 + int × X`:

An F-algebra is a type A together with a function `(1 + int × A) → A`. Unpacking the sum type, this means two functions:

- **nil case**: `1 → A` (what value do you produce for the empty list?)
- **cons case**: `int × A → A` (given an element and a partial result, what do you produce?)

Different algebras give different folds:

```cpp
// Sum algebra: A = int, nil → 0, cons(x, acc) → x + acc
auto sum = std::accumulate(vec.begin(), vec.end(), 0,
    [](int acc, int x) { return acc + x; });

// Length algebra: A = int, nil → 0, cons(_, acc) → 1 + acc
auto len = std::accumulate(vec.begin(), vec.end(), 0,
    [](int acc, int) { return acc + 1; });

// Reverse algebra: A = vector<int>, nil → {}, cons(x, acc) → acc ++ [x]
auto rev = std::accumulate(vec.begin(), vec.end(), std::vector<int>{},
    [](std::vector<int> acc, int x) { acc.push_back(x); return acc; });
```

Each `std::accumulate` call is an F-algebra in action: the initial value is the nil case, the combining function is the cons case, and the carrier type is the type of the accumulator.

## Catamorphisms: The Universal Fold

Now the deep result. The list type itself is the **initial** F-algebra — the "simplest" algebra, from which there is a **unique homomorphism** to any other algebra. This unique map is called the **catamorphism** (from Greek *kata*, "downward" — tearing down a structure).

What "initial" means concretely: for ANY algebra `(A, nil: A, cons: int × A → A)`, there is exactly ONE function `List<int> → A` that respects the structure. That function is the fold.

```cpp
template<typename T, typename A>
auto cata(const List<T>& list, A nil_case, std::function<A(T, A)> cons_case) -> A {
    return std::visit(overloaded{
        [&](const Empty&) -> A { return nil_case; },
        [&](const std::unique_ptr<Cons<T>>& c) -> A {
            return cons_case(c->head, cata(c->tail, nil_case, cons_case));
        },
    }, list);
}
```

This is THE catamorphism for lists. Every fold — sum, length, reverse, filter, map — is an instance of it with different algebra parameters.

The word "unique" is crucial. Given the algebra, the fold is *determined*. There is no choice in how to traverse the list — you must go from the inside out, applying the algebra at each layer. The structure dictates the traversal.

## Catamorphisms for Trees

For binary trees with shape `F(X) = T + X × X`:

```cpp
struct Leaf { int value; };
struct Branch;

using Tree = std::variant<Leaf, std::unique_ptr<Branch>>;

struct Branch { Tree left, right; };

template<typename A>
auto cata_tree(const Tree& t, std::function<A(int)> leaf_case,
               std::function<A(A, A)> branch_case) -> A {
    return std::visit(overloaded{
        [&](const Leaf& l) -> A { return leaf_case(l.value); },
        [&](const std::unique_ptr<Branch>& b) -> A {
            return branch_case(
                cata_tree(b->left, leaf_case, branch_case),
                cata_tree(b->right, leaf_case, branch_case));
        },
    }, t);
}
```

Different algebras:

```cpp
// Sum of all leaves
auto sum = cata_tree<int>(tree,
    [](int v) { return v; },
    [](int l, int r) { return l + r; });

// Depth of tree
auto depth = cata_tree<int>(tree,
    [](int) { return 1; },
    [](int l, int r) { return 1 + std::max(l, r); });

// Pretty print
auto show = cata_tree<std::string>(tree,
    [](int v) { return std::to_string(v); },
    [](std::string l, std::string r) { return "(" + l + " " + r + ")"; });
```

Every tree traversal is a catamorphism. Different result types, different algebras, same recursive structure.

## Anamorphisms: Building Up

The catamorphism tears down. Its dual, the **anamorphism** (from Greek *ana*, "upward"), builds up. An anamorphism starts with a seed value and unfolds it into a recursive structure.

An F-**coalgebra** is a type S (the seed) with a function `S → F(S)`. It produces one layer of structure from a seed, with new seeds in the recursive positions.

```cpp
// Anamorphism: unfold a range into a list
template<typename T, typename S>
auto ana(S seed,
         std::function<std::variant<Empty, std::pair<T, S>>(S)> step) -> List<T> {
    auto result = step(seed);
    return std::visit(overloaded{
        [](Empty) -> List<T> { return Empty{}; },
        [&](std::pair<T, S> p) -> List<T> {
            return std::make_unique<Cons<T>>(Cons<T>{p.first, ana<T>(p.second, step)});
        },
    }, result);
}

// Generate [0, 1, 2, ..., n-1]
auto iota = ana<int>(0, [](int n) -> std::variant<Empty, std::pair<int, int>> {
    if (n >= 10) return Empty{};
    return std::pair{n, n + 1};
});
```

`std::generate`, `std::iota`, and range factories are anamorphisms in disguise. They unfold a seed into a sequence.

## Hylomorphisms: Unfold Then Fold

A **hylomorphism** is an anamorphism followed by a catamorphism: build up a structure, then immediately tear it down. The structure is an intermediate representation that may be eliminated by fusion.

```
hylo = cata ∘ ana
```

Many algorithms are hylomorphisms. Mergesort:

1. **Unfold** the list into a binary tree (split into halves recursively)
2. **Fold** the tree back into a sorted list (merge sorted halves)

The tree is never actually built in an optimized implementation — the unfold and fold are *fused*. The compiler can often perform this optimization automatically when the code is structured as a composition of catamorphism and anamorphism.

## `std::visit` Is a Catamorphism

For non-recursive variants, `std::visit` is a catamorphism on a one-layer-deep structure:

```cpp
using Shape = std::variant<Circle, Rectangle, Triangle>;

auto area = std::visit(overloaded{
    [](const Circle& c)    { return pi * c.radius * c.radius; },
    [](const Rectangle& r) { return r.width * r.height; },
    [](const Triangle& t)  { return 0.5 * t.base * t.height; },
}, shape);
```

The variant `Shape` has shape functor `F(X) = Circle + Rectangle + Triangle` (no recursion). The visit provides the algebra: one function per alternative, mapping to a carrier type (double). The visit IS the catamorphism — the unique fold over this sum type.

For recursive variants (like the `Expr` type), the catamorphism requires explicit recursion through pointers, as we saw above. The pattern is the same — the recursion is just manual rather than built into `std::visit`.

## The Expression Problem Revisited

In [part 2](/post/algebraic-data-types) we introduced the expression problem. Now we can state it precisely in terms of F-algebras:

**Closed recursive types** (variants/ADTs): The shape functor F is fixed. Adding a new catamorphism (fold) is easy — just define a new algebra. Adding a new alternative to F requires changing the functor and updating all existing algebras.

**Open recursive types** (OOP/virtual): The set of algebras (virtual methods) is fixed in the base class. Adding a new "alternative" (subclass) is easy. Adding a new operation (virtual method) requires updating the base class and all subclasses.

The fundamental tension: **closed data + open operations** vs **open data + closed operations**. F-algebras give you the first. Virtual dispatch gives you the second.

Solutions that address both sides exist in type theory — tagless-final encoding, object algebras — but they add complexity. In practice, choose based on which axis of extension matters more in your domain.

## Recursive Types in Practice

### CRTP as Recursive Typing

The Curiously Recurring Template Pattern is a form of recursive typing — a class parameterized by itself:

```cpp
template<typename Derived>
class Base {
public:
    auto self() -> Derived& { return static_cast<Derived&>(*this); }
    void interface_method() { self().implementation(); }
};

class Concrete : public Base<Concrete> {
public:
    void implementation() { /* ... */ }
};
```

`Concrete` is defined in terms of `Base<Concrete>`, which references `Concrete`. The recursion is resolved through the template instantiation mechanism. This is a fixed point at the type level — `Concrete` is the type X satisfying `X = Base<X>`.

### `std::function` as Recursive Function Type

A `std::function` can hold a lambda that captures another `std::function`, creating a recursive function type:

```cpp
std::function<int(int)> factorial = [&](int n) -> int {
    return n <= 1 ? 1 : n * factorial(n - 1);
};
```

The type `std::function<int(int)>` contains (via type erasure) a closure that captures a reference to itself. This is a recursive type — a function whose implementation refers to values of its own type.

### `std::any` as Existential Recursion

`std::any` is an existential type — "there exists some type T, and I hold a value of it." When the erased type is itself a container of `std::any`, you get recursive existential types:

```cpp
using JsonValue = std::variant<
    std::nullptr_t,
    bool,
    double,
    std::string,
    std::vector<std::any>,         // array of json values
    std::map<std::string, std::any> // object of json values
>;
```

Here `std::any` hides the recursion. Each `any` is (logically) a `JsonValue`, but the self-reference goes through type erasure rather than a pointer.

## Connection to the Series

The algebraic data types from [part 2](/post/algebraic-data-types) are initial algebras of polynomial functors. The semiring arithmetic (products, sums, cardinalities) applies to each layer of the recursive structure. The polynomial functor describes one layer; the fixed point stacks infinitely many layers.

`std::visit` from [part 2](/post/algebraic-data-types) is a catamorphism for non-recursive sums. `std::accumulate` is a catamorphism for lists. Every fold, every reduce, every recursive visitor — all catamorphisms.

The substructural discipline from [part 8](/post/substructural-types) applies to recursive types too: each `unique_ptr` in a recursive variant enforces affine (move-only) ownership of sub-trees, preventing shared mutable aliases.

## Closing: Data Is Algebra

Recursive data structures are not ad-hoc programming constructs. They are fixed points of polynomial functors. The operations on them — folds, unfolds, maps, traversals — are algebraic homomorphisms with precise mathematical characterizations.

When you write `std::accumulate`, you are computing a catamorphism. When you write a recursive `std::visit`, you are computing a catamorphism on a recursive sum type. When you generate a sequence from a seed, you are computing an anamorphism.

The formal machinery is not necessary for writing correct code. But it gives you a vocabulary for *understanding* code — for recognizing that two seemingly different algorithms are instances of the same pattern, for knowing when optimizations (like hylomorphism fusion) are valid, and for designing data structures whose operations are determined by their shape.

In the [final post](/post/type-safe-protocol), we combine every technique from this series — algebraic types, phantom types, typestate, concepts, compile-time data, parametricity, substructural types, and recursive structure — into a complete, compile-time verified protocol stack.
