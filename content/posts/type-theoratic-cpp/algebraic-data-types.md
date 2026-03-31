---
title: "Sum Types and Product Types — The Algebra of C++ Types"
date: 2026-03-21
slug: algebraic-data-types
tags: [c++20, type-theory, variant, tuple, algebraic-types, initial-algebras]
excerpt: "Types form a semiring. Products multiply, sums add, and the distributive law lets you factor types like polynomials. Initial algebras, catamorphisms, and the deep reason why std::visit is the only operation you need."
---

In the [previous post](/post/type-theoretic-foundations) we established a principle: types are not just containers for data — they encode truths about a domain. We introduced formation, introduction, and elimination rules. Now we need a vocabulary for *combining* types. That vocabulary is algebra — the literal algebra of types, where the arithmetic of natural numbers shows up in the structure of your data.

I want to be clear: this is not a metaphor. This is not one of those loose analogies where someone says "monads are like burritos" and then you spend the next three years confused about both monads and burritos. Types form an actual mathematical structure called a **semiring**, with the same laws as natural number arithmetic. The plus is real. The times is real. The distributive law is real. And understanding this algebra changes how you design types, how you spot redundancy, and how you recognize the fundamental operations on data.

This post builds on [post #11 on std::variant](/post/variant-visit-and-sum-types) from the C++ series. If you are comfortable with `std::variant` and `std::visit`, you have the tools. This post gives you the theory behind them — and once you have the theory, you will start seeing algebra everywhere. In your structs. In your function signatures. In your error handling. In the pattern of dead fields haunting your data structures like ghosts that only appear in production.

## The Arithmetic of Types

Every type has a *cardinality* — the number of distinct values it can hold. `bool` has cardinality 2 (true or false — the entire universe of moral certainty, compressed into a single bit). An `enum` with four enumerators has cardinality 4. A `uint8_t` has cardinality 256. This is not abstract nonsense — it is literally counting: how many different things can this type be?

The cardinality follows arithmetic rules, and the rules are exact. Not approximately correct. Not "it is kind of like arithmetic." The correspondence is exact down to the last value.

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

This is why structs are called **product types**. The name is not decorative. `std::pair<A, B>` has cardinality |A| × |B|. `std::tuple<A, B, C>` has cardinality |A| × |B| × |C|. Every time you write a struct, you are computing a multiplication problem whether you know it or not.

Every field you add to a struct *multiplies* the state space. Adding a `bool` doubles it. Adding a five-variant enum quintuples it. Adding a `std::string` technically multiplies it by infinity, which is fine mathematically but should give you pause architecturally.

Here is an exercise that will ruin the way you look at code forever: go to a struct in your codebase and multiply the cardinalities of its fields. Now ask: how many of those values actually make sense in the domain? If the answer is "a tiny fraction," congratulations — you have discovered that your type is a liar. It claims to represent your domain, but most of its inhabitants are nonsense states that your code must carefully avoid constructing.

```cpp
struct UserAccount {
    std::string username;        // ∞
    std::string email;           // ∞
    bool is_verified;            // 2
    bool is_admin;               // 2
    std::optional<Date> deleted; // Date + 1
};
```

The cardinality here is staggering. But the *meaningful* cardinality is much smaller — an unverified admin is suspicious, a deleted user who is also an admin is a security concern, and a user whose email is "DROP TABLE users" is a penetration test. The product type admits all of these without blinking.

We will come back to this problem. For now, remember the rule: **every field you add to a struct multiplies the number of values you have to worry about.**

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

Variants are **sum types**. The cardinality is the *sum* of the alternatives. Where product types say "I have ALL of these," sum types say "I have ONE of these." The product is an AND. The sum is an OR.

This distinction matters far more than it might seem at first. Product types are generous — they give you everything at once, whether you need it or not. Sum types are precise — they give you exactly one thing, and they tell you which thing it is. Products overcommit. Sums commit exactly once.

If you have used a union in C, you have already encountered a sum type, except the C version is an untagged sum — it does not remember which alternative is active, so you can read a `float` out of memory that was written as an `int` and get a number that means nothing in any base. `std::variant` is a *tagged* sum — it remembers, and it prevents you from reading the wrong alternative. The tag is the difference between a sum type and a footgun.

### The Unit and the Void

At the extremes of the cardinality spectrum, two types sit quietly, doing more work than anyone gives them credit for.

**The unit type** has exactly one value: `std::monostate` or an empty struct. Cardinality 1. It carries no information — or more precisely, it carries exactly one bit of information, which is "I exist." It is the multiplicative identity: `A × 1 ≅ A`. A struct with a monostate field is the same as a struct without it, because the monostate adds zero information. It is the type-theoretic equivalent of multiplying by one.

Why does the unit type exist? Because sometimes you need to say "something happened" without saying what. A `std::variant<Success, Error>` where `Success` is an empty struct is perfectly valid — the success case carries no data beyond the fact that it succeeded. In callback systems, `void` often wants to be the unit type, but C++ made `void` special and irregular (you cannot have a `std::vector<void>` or a `std::variant<void, Error>`). `std::monostate` exists to fill this gap, and it fills it perfectly.

**The void type** (in the type-theoretic sense, not C++'s `void`) has zero values — uninhabitable. `std::variant<>` (a variant with no alternatives) would be the true void type. Cardinality 0. It is the additive identity: `A + 0 ≅ A`. Adding an uninhabitable alternative to a variant changes nothing, because that alternative can never be active.

The void type is less practically useful than the unit type, but it shows up in theoretical contexts constantly. A function that returns the void type can never return — it must loop forever or throw. A product with a void field is itself void (`A × 0 = 0`), because you can never construct the void field. The void type is the black hole of type theory: anything that touches it becomes uninhabitable.

In C++, the closest thing to a true void type is a class with a deleted constructor and no friends. It exists as a concept but can never be instantiated, which is exactly the point.

## The Semiring of Types

Here is where it gets remarkable. Types under sum (+) and product (×) satisfy the same laws as natural numbers under addition and multiplication. This structure is called a **semiring**, and the fact that it works is one of those results that seems too good to be true until you verify every law and find that they all hold.

Let me walk through the laws. Each one has a direct, practical consequence for how you design types.

### Additive identity
```
A + 0 ≅ A
```
A variant with an uninhabitable alternative is isomorphic to a variant without it. Adding "nothing" changes nothing. This is the type-theoretic way of saying: if one of your variant alternatives can never happen, delete it. It is dead code in type form.

### Multiplicative identity
```
A × 1 ≅ A
```
A struct with a unit field is isomorphic to the struct without it. A field carrying no information can be dropped. If you have a struct with a `std::monostate` field, you can remove it. More commonly, if you have a struct with a `bool` field that is always `true` in practice, that field is *effectively* a unit — its information content is zero, and it exists only to confuse people during code review.

### Absorbing element
```
A × 0 ≅ 0
```
A struct with an uninhabitable field is itself uninhabitable. If one component can never be provided, the whole product can never be constructed. This is the algebraic explanation for why you cannot create an instance of a struct that contains a field of a type with a deleted constructor. The zero absorbs everything it touches.

This also explains a common source of confusion with `std::variant`. If you accidentally include an uninhabitable type in a variant, it does not make the variant uninhabitable — it just makes that alternative unreachable. But if you include an uninhabitable type in a struct, the struct itself becomes unconstructible. Sums tolerate zeros; products are destroyed by them. Addition absorbs nothing; multiplication absorbs everything.

### Commutativity
```
A + B ≅ B + A
A × B ≅ B × A
```
`variant<int, string>` and `variant<string, int>` are isomorphic — they hold the same set of values, just in different order. Similarly for pairs. The order of fields in a struct does not change the set of representable values (though it may change the memory layout, because C++ has opinions about padding that type theory does not).

In practice, this means that if you are choosing between `pair<A, B>` and `pair<B, A>`, pick whichever is more readable. The algebra does not care.

### Associativity
```
(A + B) + C ≅ A + (B + C)
(A × B) × C ≅ A × (B × C)
```
Nesting does not change the value space. `pair<pair<A,B>, C>` and `tuple<A, B, C>` are isomorphic. `variant<variant<A, B>, C>` and `variant<A, B, C>` are isomorphic (modulo the fact that C++ does not flatten nested variants automatically, which is annoying but not algebraically significant).

This is why `std::tuple` exists as a flat product instead of a nested pair tower. In Haskell and ML, pairs and tuples are the same thing with different syntax. C++ draws a sharper distinction for practical reasons, but the algebra does not care how deep the nesting goes.

### The Distributive Law

This is the powerful one. This is the one you will actually use to refactor types:

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

These two representations are *isomorphic* — they hold exactly the same information. But they have different ergonomics. The factored form (`Request`) is compact — the `UserId` lives in one place. The distributed form (`Request2`) is explicit — each variant alternative is self-contained, and you cannot accidentally access `PostBody` on a GET request because it simply does not exist in the `GetRequest` struct.

This is type-level factoring. Just as `3 × (5 + 7) = 3 × 5 + 3 × 7` in arithmetic, you can factor or distribute type expressions. And just like in arithmetic, sometimes the factored form is cleaner and sometimes the expanded form is cleaner. The algebra gives you the freedom to choose.

Here is a heuristic that has served me well: if every branch of your code that handles the variant immediately accesses the shared fields, keep the factored form — the shared fields are genuinely shared. If different branches do fundamentally different things with the surrounding data, distribute — let each alternative carry exactly what it needs.

### Beyond the basics: exponentiation

The semiring gives us addition and multiplication. But types also have a notion of *exponentiation*: function types.

```
B^A ≅ A → B
```

The number of functions from A to B is |B|^|A|. A function from `bool` to `int` (where `int` has n values) has n² possible implementations — one output choice for `true`, one for `false`. A function from `uint8_t` to `bool` has 2^256 possible implementations — for each of the 256 inputs, you choose true or false independently.

This is why function types are called **exponential types**. And it leads to a beautiful identity:

```
C^(A+B) ≅ C^A × C^B
```

A function from a sum type is isomorphic to a product of functions. If you have a function `variant<A, B> → C`, it is equivalent to having two functions: one from `A → C` and one from `B → C`. This is exactly what `std::visit` with the `overloaded` pattern does — it takes a "function from a variant" and decomposes it into separate handlers, one per alternative.

The identity `C^(A+B) = C^A × C^B` is why pattern matching is the natural elimination form for sum types. You are not making an arbitrary design choice when you handle each case separately — you are following the exponential law. The algebra demands it.

There is also:

```
(C^B)^A ≅ C^(A×B)
```

A function that takes an A and returns a function that takes a B and returns a C is the same as a function that takes a pair (A, B) and returns a C. This is **currying**, and it falls directly out of the algebra. Haskell Curry did not invent currying so much as *discover* it — the arithmetic of types made it inevitable.

## `std::optional<T>` Is `T + 1`

`std::optional<T>` is a sum type with two alternatives: a T or nothing. "Nothing" is the unit type (one value: the empty state). So `optional<T>` has cardinality |T| + 1.

This is the algebraic way of saying: optional adds one extra state — the null state. Just one. Not a thousand possible null-related bugs. One extra state that the type system tracks and the compiler enforces.

This means `optional` obeys the algebra:

```
optional<optional<T>> ≅ T + 1 + 1 ≅ T + 2
```

A nested optional has three states: empty-outer, empty-inner, or a T. But `optional<optional<T>>` distinguishes `nullopt` (empty outer) from `optional<T>{nullopt}` (non-empty outer containing empty inner). This is algebraically correct: `(T + 1) + 1 = T + 2`, and the two `+1`s are distinct.

If this feels pedantic, consider a practical case: a configuration system where a setting can be "not set" (use default), "explicitly set to nothing" (disable this feature), or "set to a value" (use this value). That is three states. `optional<optional<T>>` captures them perfectly. A single `optional<T>` cannot — it conflates "not set" with "set to nothing." The algebra does not lie.

You can also see `optional<bool>` through this lens:

```
optional<bool> ≅ bool + 1 ≅ 2 + 1 = 3
```

Three values: `nullopt`, `true`, `false`. This is SQL's three-valued logic in a type. If you have ever been bitten by a nullable boolean column in a database, you have experienced `bool + 1` without the benefit of the algebra to warn you.

### The bool trap

While we are doing arithmetic with small types, let us talk about the most abused type in programming:

```
bool × bool = 2 × 2 = 4
```

Four values. But in practice, most uses of `pair<bool, bool>` — or more commonly, functions with two bool parameters — only intend two or three of those four states. The fourth state is the bug.

```cpp
auto open_file(std::string path, bool writable, bool create);
```

Is `open_file("x", true, false)` meaningful? What about `open_file("x", false, true)` — open a read-only file that does not exist yet, but create it? The product `bool × bool` admits four combinations, but the domain probably only has three meaningful ones (read-existing, write-existing, write-create). The fourth is a "valid" combination that produces nonsensical behavior.

The fix is a sum type:

```cpp
enum class OpenMode { ReadExisting, WriteExisting, WriteCreate };
auto open_file(std::string path, OpenMode mode);
```

Cardinality 3. Exactly the right size. No meaningless combinations. The algebra caught the bug before it happened.

## Why Enums Are Weak Sum Types

C++ enums look like sum types but are *degenerate* — each alternative carries no data:

```cpp
enum class Shape { Circle, Rectangle, Triangle };  // 1 + 1 + 1 = 3
```

This is a sum of units: three alternatives, each carrying zero information beyond "I am this alternative." It is like a multiple-choice question where the answer is A, B, or C, and you are not allowed to show your work.

To associate data with each case, the pre-variant approach was a product-pretending-to-be-a-sum. I call this the "struct of lies" pattern:

```cpp
struct ShapeData {
    Shape kind;
    double radius;    // only valid if Circle
    double width;     // only valid if Rectangle
    double height;    // only valid if Rectangle or Triangle
};
```

This has cardinality `3 × ℝ × ℝ × ℝ` — massively larger than the domain requires. Most inhabitants are contradictions. What is a Rectangle with a radius of 7.5? What is a Circle with width 3 and height 4? These values exist in the type. They compile. They can be constructed. They are nonsense.

The `kind` field is doing the work of a variant tag, but without any of the guarantees. Nothing prevents you from reading `radius` when `kind` is `Rectangle`. Nothing prevents you from setting `kind` to `Circle` and then populating `width` and `height`. The struct lies about its contents in the same way a trench coat lies about the number of people underneath it.

The variant version:

```cpp
struct Circle { double radius; };
struct Rectangle { double width, height; };
struct Triangle { double base, height; };

using Shape = std::variant<Circle, Rectangle, Triangle>;
```

Cardinality: `ℝ + ℝ² + ℝ²` — exactly the right size. No dead fields, no invalid combinations, no trench coat. A `Circle` has a radius and nothing else. A `Rectangle` has width and height and nothing else. The tag (which alternative is active) is managed by the variant, not by your discipline.

I want to belabor this point because I have seen the "struct of lies" pattern in approximately every codebase I have ever worked in. It is the default way C programmers model tagged unions, and C++ programmers inherit the habit. It *works* — the code runs, the tests pass, production is fine — until someone constructs an invalid combination and you spend three days debugging a shape that thinks it is simultaneously a circle and a rectangle.

The algebra makes the problem obvious: `3 × ℝ³ ≠ ℝ + ℝ² + ℝ²`. The left side is a product. The right side is a sum. They have different cardinalities. The product is larger. The excess inhabitants are bugs.

## The Overloaded Pattern

The essential tool for working with sum types in C++:

```cpp
template<typename... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
```

One line. Possibly the highest value-per-character ratio of any line of C++ ever written. It inherits from all the lambdas and brings their `operator()` into scope, creating a single callable object that dispatches based on the argument type. Combined with class template argument deduction:

```cpp
auto area(const Shape& s) -> double {
    return std::visit(overloaded{
        [](const Circle& c)    { return std::numbers::pi * c.radius * c.radius; },
        [](const Rectangle& r) { return r.width * r.height; },
        [](const Triangle& t)  { return 0.5 * t.base * t.height; },
    }, s);
}
```

If you add a `Pentagon` and forget to handle it, the compiler rejects the program. Not a warning. Not a runtime `default` branch that logs "unexpected shape" and returns zero. A hard compiler error. The elimination rule is enforced at compile time, which is the only time enforcement actually matters — runtime enforcement is just damage control.

This pattern is so central to working with variants that I genuinely think it should be in the standard library. It was proposed for C++26 and may eventually arrive, but until then, every codebase that uses `std::variant` has this exact two-line definition somewhere, usually in a header called `overloaded.hpp` or `visit_helpers.hpp` or `why_isnt_this_in_the_standard.hpp`.

### How the overloaded pattern works

For the curious: the `...` in `Ts...` is a parameter pack expansion. `struct overloaded : Ts...` means "inherit from every type in the pack." `using Ts::operator()...` means "bring every base class's `operator()` into scope." The result is a class with N overloaded call operators — one for each lambda you gave it.

When `std::visit` calls this object with a `Circle`, overload resolution picks the lambda that takes `const Circle&`. When called with a `Rectangle`, it picks the `const Rectangle&` lambda. And so on. The dispatch is resolved at compile time. There is no virtual table, no runtime type check, no hash map of handlers. Just good old-fashioned overload resolution doing exactly what it was designed to do.

The template argument deduction guide (which is implicit in C++20) lets you write `overloaded{lambda1, lambda2, lambda3}` instead of `overloaded<decltype(lambda1), decltype(lambda2), decltype(lambda3)>{lambda1, lambda2, lambda3}`, which is a mercy for which we should all be grateful.

## Initial Algebras: Why ADTs Are Special

Now for the deeper theory. Why are algebraic data types so natural? Why does `std::visit` feel like *the* fundamental operation on sum types? Why does folding feel like *the* fundamental operation on lists? The answer comes from a concept called **initial algebras**, and it is one of those ideas that makes everything click.

An **algebra** for a type constructor F is a type A together with a function `F(A) → A`. Think of F as the "shape" of one layer of your data, and the algebra as a way to "collapse" one layer into a result.

That sounds abstract. Let us make it concrete.

For a list of integers, the shape functor is `F(X) = 1 + int × X` — either empty (1) or an element and a tail (int × X). Now, an algebra for this functor is *any* type A together with a function `(1 + int × A) → A`. Unpacking, this means two things:

1. A way to handle the empty case — a value of type A (what do you return when the list is empty?)
2. A way to handle the cons case — given an int and an A, produce another A (given the head element and the result of processing the tail, what do you return?)

Sound familiar? It should. This is exactly the signature of `std::accumulate`:

```cpp
std::accumulate(begin, end, init, combine);
//                          ↑         ↑
//                     empty case   cons case
```

The initial value `init` handles the empty case. The binary function `combine` handles the cons case. Together, they form an algebra for the list functor.

Here is the remarkable fact: the list type *itself* is the **initial algebra** — the "simplest" algebra for this functor, from which there is a unique function (called a **catamorphism**) to any other algebra.

What does "initial" mean here? It means that for ANY algebra you can dream up — any choice of result type A, any initial value, any combining function — there is *exactly one* way to fold the list into that algebra. Not "at most one." Exactly one. The fold is determined by the algebra. The structure of the list determines the recursion pattern. You just supply the pieces.

```cpp
// Sum: the algebra is (0, λ(x, acc). x + acc)
auto sum = std::accumulate(vec.begin(), vec.end(), 0);

// Length: the algebra is (0, λ(_, acc). 1 + acc)
auto len = std::accumulate(vec.begin(), vec.end(), 0,
    [](int acc, int) { return acc + 1; });

// To string: the algebra is ("", λ(x, acc). acc + to_string(x))
auto str = std::accumulate(vec.begin(), vec.end(), std::string{},
    [](std::string acc, int x) { return acc + std::to_string(x) + ","; });

// Product: the algebra is (1, λ(x, acc). x * acc)
auto prod = std::accumulate(vec.begin(), vec.end(), 1, std::multiplies<>{});

// Maximum: the algebra is (INT_MIN, λ(x, acc). max(x, acc))
auto mx = std::accumulate(vec.begin(), vec.end(), INT_MIN,
    [](int acc, int x) { return std::max(acc, x); });
```

Every `std::accumulate` call is a catamorphism — the unique homomorphism from the initial algebra (the list) to your target algebra (the result type with your combining function). Different algebras = different folds. The *structure* is always the same: peel off one layer, apply the algebra, recurse. The word "catamorphism" comes from the Greek *kata* (downward) — you are tearing down a structure level by level, like disassembling a stack of plates.

### Why does this matter?

Because it means that `fold` (or `accumulate`, or `reduce`, or whatever your language calls it) is not one operation among many. It is *the* operation on lists. Every other operation — sum, length, map, filter, reverse, flatten — can be expressed as a fold. The initial algebra theorem guarantees this: there is a unique catamorphism to any target, so any function that processes a list one element at a time is a fold, whether it knows it or not.

For non-recursive sum types (plain variants), the catamorphism degenerates to `std::visit` — a one-layer fold. You "tear down" the variant by providing a handler for each alternative. The variant has no recursive structure to traverse, so the fold is just a single dispatch. But the principle is the same: the elimination rule for a sum type is *determined* by the initial algebra structure. You do not choose how to consume sum types; the algebra chooses for you.

This is deeply satisfying if you like your programming to have a sense of inevitability. You are not inventing patterns. You are discovering them.

### Catamorphisms for trees

Lists are not the only initial algebra. Any recursive data type is one. Consider a binary tree:

```
Tree<A> = A + Tree<A> × Tree<A>
```

A tree is either a leaf carrying an A, or a branch with two subtrees. The shape functor is `F(X) = A + X × X`. An algebra for this functor is a type R with:

1. A function `A → R` (how to handle a leaf)
2. A function `R × R → R` (how to combine the results of two subtrees)

The unique catamorphism folds the tree bottom-up:

```cpp
template<typename A, typename R>
auto fold_tree(const Tree<A>& t, std::function<R(A)> leaf, std::function<R(R, R)> branch) -> R {
    return std::visit(overloaded{
        [&](const Leaf<A>& l) { return leaf(l.value); },
        [&](const Branch<A>& b) {
            return branch(fold_tree(*b.left, leaf, branch),
                          fold_tree(*b.right, leaf, branch));
        },
    }, t);
}

// Sum of all leaves
auto total = fold_tree(tree, std::identity{}, std::plus<>{});

// Height of the tree
auto height = fold_tree<int, int>(tree,
    [](int) { return 0; },
    [](int l, int r) { return 1 + std::max(l, r); });

// Collect all leaves into a vector
auto leaves = fold_tree<int, std::vector<int>>(tree,
    [](int x) { return std::vector{x}; },
    [](auto l, auto r) { l.insert(l.end(), r.begin(), r.end()); return l; });
```

Every tree traversal is a catamorphism. Depth, size, sum, search, serialization — they are all folds. The initial algebra for trees gives you the recursion pattern; you supply the algebra (the base case and the combining step). Nothing else is needed.

## Polynomial Functors: Types as Equations

The shape of an ADT can be written as a polynomial. This is not a loose analogy — it is the actual mathematical structure:

```
List<A>  = μX. 1 + A × X          (empty or cons)
Tree<A>  = μX. A + X × X          (leaf or branch)
Nat      = μX. 1 + X              (zero or successor)
Maybe<A> = 1 + A                   (nothing or just)
Either<A,B> = A + B                (left or right)
```

The `μX` is the "fixed point" operator — it ties the recursive knot. `List<A>` is the smallest type X satisfying `X = 1 + A × X`. "Smallest" is doing heavy lifting here — it means the type contains only values that can be constructed in finitely many steps. No infinite lists. (Haskell, being lazy, admits infinite lists as inhabitants of its list type. C++ does not have this problem, or this luxury, depending on your perspective.)

Now watch this. If we treat the equation algebraically and "solve" for X:

```
X = 1 + A × X
X - A × X = 1
X × (1 - A) = 1
X = 1 / (1 - A)
```

We just subtracted types and divided types, which makes no sense. There is no "type subtraction." There is no "type division." We should not be allowed to do this. And yet:

`1 / (1 - A)` is the geometric series: `1 + A + A² + A³ + ...`

A list of A is: either empty (1), or one element (A), or two elements (A × A = A²), or three elements (A × A × A = A³), and so on. The algebraic solution *is* the type. The formal power series expansion gives you the cardinality of each "layer" of the recursive type.

We committed several mathematical crimes to get here — subtracting and dividing types as if they were numbers — but the answer is correct. This is not numerology. It is a real correspondence between combinatorics and type theory, studied by Joyal, Fiore, and others under the name **combinatorial species**. The algebra of types is isomorphic to the algebra of formal power series, and the formal manipulations, despite being "illegal," produce correct results.

### Differentiating types (no, really)

If you found the above suspicious, hold on, because it gets wilder. You can take the *derivative* of a type.

The derivative of a type `T` with respect to a type variable `A` gives you the type of "one-hole contexts" — a `T` with exactly one `A` removed, leaving a hole. This is called the **zipper** of the type.

```
d/dA (A × A) = A + A = 2A
```

The derivative of a pair of A's is "a pair with one A missing" — you keep the other A, and you remember which position the hole is in (left or right). Two positions, so two terms.

```
d/dA (List<A>) = List<A> × List<A>
```

The derivative of a list is a pair of lists — the elements before the hole and the elements after the hole. This is exactly Huet's zipper, a well-known functional data structure for navigating and editing lists and trees.

We derived the zipper by differentiating a type with a pencil on paper. If that does not make you at least slightly unsettled about the nature of mathematics, I do not know what will.

### Why is the notation `μX`?

The `μ` in `μX. F(X)` stands for the *least fixed point* of the type equation `X = F(X)`. It comes from recursion theory, where `μ` is the minimization operator. The notation says: "find the smallest X such that X equals F(X)." For lists, this is the smallest X satisfying `X = 1 + A × X`, which is the set of all finite lists.

The *greatest* fixed point, written `νX. F(X)`, gives you the type of potentially infinite structures (streams, infinite trees). Haskell's list type is the greatest fixed point. C++'s `std::vector` is the least fixed point. The distinction matters if you care about laziness and corecursion, which C++ generally does not, but type theory does.

## Products with Invariants

A naked product with public fields makes no promises. It is an open buffet, and your code is the unsupervised toddler:

```cpp
struct DateRange {
    Date start;
    Date end;  // could be before start!
};
```

Cardinality: `|Date|²`. But the meaningful state space is roughly `|Date|² / 2` — only half the pairs satisfy `start ≤ end`. The other half are time-traveling contradictions.

This permits `start > end` — a contradiction in the domain. The product is too large. You have more inhabitants than meaningful states, and the excess inhabitants are not just useless — they are actively harmful. They are the states your code will encounter at 3 AM on a Saturday when the on-call engineer is asleep.

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

In type-theoretic terms: `DateRange` is a **refinement type** — a subset of `Date × Date` where the invariant `start ≤ end` holds. The private constructor restricts the introduction rule to only admit valid inhabitants. The factory function returns `optional<DateRange>` — the sum type `DateRange + 1` — making the possibility of failure explicit in the type.

This pattern — private data, smart constructor, public getters — is the C++ programmer's refinement type. It is not as powerful as dependent types (which can express arbitrary predicates in the type system), but it is surprisingly effective. Every refinement type you write shrinks the gap between "values the type admits" and "values the domain allows."

### When refinement is overkill

Not every struct needs invariant enforcement. If a struct is a plain data carrier — a DTO, a config record, a named tuple — and every combination of field values is meaningful, then public fields are fine. The product type is exactly the right size, and hiding it behind getters and setters adds ceremony without adding safety.

The heuristic: if you can construct *any* instance and it makes sense in the domain, use a struct with public fields. If some combinations are nonsense, either use a sum type (to eliminate the bad combinations structurally) or a class with a smart constructor (to validate at construction time). Do not use a struct with public fields and rely on documentation to ward off invalid states. Documentation is the type-theoretic equivalent of a "please do not step on the grass" sign.

## Sums for Error Handling

`std::optional<T>` is `T + 1` — a value or nothing. But sometimes "nothing" is insufficient. You need to know *why* the operation failed. "Nothing" does not help you fix the bug. "Nothing" does not show up in the error log. "Nothing" is the shrug emoji of error handling.

What you want is a sum of success and failure, where failure carries information:

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

The caller must handle both cases — `std::visit` enforces exhaustiveness. This is `T + E`, the sum of success and failure. Compare to returning `-1` (a lie: the error is disguised as a valid value, hiding inside the type like a spy), or throwing (invisible in the signature: the caller does not know failure is possible unless they read the documentation, which they will not). The variant makes the disjunction explicit and impossible to ignore.

### The error hierarchy

You can use the algebra to build rich error types:

```cpp
struct NetworkError { int code; std::string host; };
struct ParseError { std::string message; size_t position; };
struct ValidationError { std::string field; std::string reason; };

using AppError = std::variant<NetworkError, ParseError, ValidationError>;
using AppResult = std::variant<Response, AppError>;
```

This is `Response + (NetworkError + ParseError + ValidationError)` = `Response + NetworkError + ParseError + ValidationError` (by associativity of sums). Each error type carries exactly the data relevant to that kind of failure. No `error_code` enum with a `message` string where the message is only populated for some codes. No `std::exception` base class with a `what()` that returns a string you have to parse to figure out what went wrong.

The algebra gives you precision. Each alternative in the sum carries exactly the data it needs, no more and no less. The compiler forces you to handle every kind of error. And the type signature tells you exactly which errors are possible, which is information that exceptions hide and error codes obscure.

C++23's `std::expected<T, E>` formalizes this pattern with nicer syntax and monadic operations (`.and_then()`, `.transform()`, `.or_else()`), but the algebraic structure is identical: `T + E`.

## The Expression Problem

There is a deep tension in how you organize data, and it has type-theoretic roots. It is one of those problems that seems like a practical annoyance until you realize it is fundamental.

**Sum types** (variants/ADTs) make adding new *operations* easy — just write a new `std::visit` with a new set of cases. But adding a new *variant* is hard: every existing visitor must be updated. If you add `Pentagon` to the `Shape` variant, every function that visits shapes must learn about pentagons. The compiler forces this, which is good (no missed cases), but it means adding a variant is a cross-cutting change that touches many files.

**Subtype polymorphism** (virtual functions/OOP) makes adding new *variants* easy — just write a new subclass. Nobody else needs to change. But adding a new *operation* is hard: every existing class must be updated with a new virtual method. If you add a `perimeter()` method to the `Shape` base class, every subclass must implement it. Adding an operation is a cross-cutting change.

This is the **expression problem**, named by Philip Wadler (yes, the same Wadler — the man is everywhere). It is not a practical annoyance — it is a fundamental duality in type theory. Sum types give you *closed* data (fixed set of alternatives) with *open* operations. Class hierarchies give you *open* data (new subclasses anytime) with *closed* operations (fixed set of virtual methods).

Neither is universally better. The right choice depends on which axis of extension matters more in your domain:

- **Compilers**: expression types are fixed (you rarely add new syntax), but operations multiply (optimization passes, type checkers, code generators, pretty printers, linters). Use ADTs.
- **GUI frameworks**: operations are fixed (draw, resize, focus, handle-input), but widget types multiply (buttons, sliders, text fields, custom widgets). Use classes.
- **Game engines**: entity types multiply (new enemy types, new item types), but operations are somewhat fixed (render, update, collide). Use classes, or better yet, an ECS.
- **Serialization libraries**: the set of types to serialize grows (user-defined types), but the operations are fixed (serialize, deserialize). Use classes or type classes.

The expression problem is not solvable — you cannot have both axes of extension be easy simultaneously without some form of boilerplate or advanced type system feature (like Scala's multimethods or Haskell's type classes). But knowing the problem exists helps you make the right choice upfront instead of discovering it the hard way when your codebase has grown around the wrong abstraction.

## Isomorphisms and Canonical Forms

Two types with the same cardinality are **isomorphic** — there exists a pair of functions converting between them without losing information. You can go from A to B and back to A, arriving exactly where you started.

```
std::pair<bool, A> ≅ std::variant<A, A>
```

Because `2 × |A| = |A| + |A|`. A pair of a bool and an A is the same information as choosing between two copies of A. The bool selects which "copy" you are in. You can convert freely:

```cpp
// pair<bool, A> → variant<A, A>
template<typename A>
auto to_variant(std::pair<bool, A> p) -> std::variant<A, A> {
    if (p.first) return std::variant<A, A>(std::in_place_index<0>, std::move(p.second));
    else return std::variant<A, A>(std::in_place_index<1>, std::move(p.second));
}

// variant<A, A> → pair<bool, A>
template<typename A>
auto to_pair(std::variant<A, A> v) -> std::pair<bool, A> {
    return std::visit(overloaded{
        [](std::in_place_index_t<0>, A&& a) { return std::pair{true, std::move(a)}; },
        [](std::in_place_index_t<1>, A&& a) { return std::pair{false, std::move(a)}; },
    }, std::move(v));
}
```

When you spot an isomorphism, you can choose whichever representation is more ergonomic. The algebra tells you they are equivalent, so the choice is purely pragmatic — which one reads better? Which one works better with your existing APIs?

Here are some useful isomorphisms to keep in your pocket:

```
A × B ≅ B × A                             // field order does not matter
A + B ≅ B + A                             // variant order does not matter
(A × B) × C ≅ A × (B × C) ≅ tuple<A,B,C> // nesting does not matter
(A + B) + C ≅ A + (B + C) ≅ variant<A,B,C> // nesting does not matter
A × (B + C) ≅ (A × B) + (A × C)           // distribute or factor
A × 1 ≅ A                                  // drop useless fields
A + 0 ≅ A                                  // drop impossible alternatives
pair<bool, A> ≅ variant<A, A>              // flag + value ≅ tagged union
optional<A> ≅ variant<monostate, A>         // optional is a variant
```

These are refactoring tools. If you see a struct with a bool flag where each branch uses different subsets of the remaining fields, the `pair<bool, A> ≅ variant<A, A>` isomorphism tells you: split into a variant. If you see a variant where every alternative shares a common field, the distributive law tells you: factor it out into a surrounding struct. The algebra gives you permission — and a proof of correctness — for these transformations.

## Recursive Types in C++

Recursive variants need indirection because C++ must know sizes at compile time. You cannot put an `Expr` inside an `Expr` directly because the size of `Expr` depends on the size of `Expr`, which depends on the size of `Expr`, which depends on — you see the problem. It is turtles all the way down, and C++ needs the turtles to stop.

The solution is `std::unique_ptr`, which has a fixed size (one pointer) regardless of what it points to:

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

In the polynomial functor notation:

```
Expr = double + Expr + Expr × Expr
```

A literal is a leaf. Negation is a unary node. Addition is a binary node. This is a polynomial in `Expr` — specifically, a sum of products, which is all an ADT ever is.

Evaluating is a recursive catamorphism — we tear the expression tree down layer by layer:

```cpp
auto eval(const Expr& e) -> double {
    return std::visit(overloaded{
        [](const Literal& lit) { return lit.value; },
        [](const std::unique_ptr<Negate>& n) { return -eval(n->operand); },
        [](const std::unique_ptr<Add>& a) { return eval(a->left) + eval(a->right); },
    }, e);
}
```

No dynamic dispatch. No inheritance hierarchy. No virtual destructor that you forgot to add and now there is a memory leak on every `delete` of a base pointer. Just types and catamorphisms. The visitor handles each case, the recursion handles the depth, and the variant enforces that you handle every node type.

### Adding operations vs adding nodes

This expression type is a sum type, so adding operations is easy. Want a pretty-printer? Write another `std::visit`:

```cpp
auto to_string(const Expr& e) -> std::string {
    return std::visit(overloaded{
        [](const Literal& lit) { return std::to_string(lit.value); },
        [](const std::unique_ptr<Negate>& n) { return "-(" + to_string(n->operand) + ")"; },
        [](const std::unique_ptr<Add>& a) {
            return "(" + to_string(a->left) + " + " + to_string(a->right) + ")";
        },
    }, e);
}
```

Want a constant folder? Another `std::visit`. Want a tree flattener? Another `std::visit`. Each new operation is a standalone function. No existing code changes.

But adding a new node type — say, `Multiply` — requires updating every existing visitor. This is the expression problem rearing its head again. In a compiler, where expression types are usually fixed, this trade-off works beautifully. In a plugin system where users can add new expression types, it does not.

We will explore recursive types, fixed points, and the full menagerie of catamorphisms, anamorphisms, and hylomorphisms in [part 10](/post/recursive-types-and-fixed-points).

## In Loom

Loom's HTTP layer is built on sum types. Every major data structure uses variants instead of products-with-tags, eliminating dead fields and making the compiler enforce exhaustive handling. This is not ideological purity — it is the result of getting burned by the "struct of lies" pattern one too many times and deciding that the compiler should do the work instead of the programmer's memory.

### Connection as a Sum Type

A connection is either reading a request or writing a response. The old design (which existed for approximately one afternoon before I refactored it in a fit of algebraic indignation) would be a struct with a state flag and fields for both phases — most fields dead in any given state. Instead:

```cpp
struct ReadPhase  { std::string buf; };
using WritePhase = std::variant<OwnedWrite, ViewWrite>;

struct Connection {
    std::variant<ReadPhase, WritePhase> phase{ReadPhase{}};
    bool keep_alive = true;
    int64_t last_activity_ms = 0;
};
```

Cardinality analysis: the `phase` field is `ReadPhase + WritePhase`, which is `ReadPhase + OwnedWrite + ViewWrite` (by associativity). The product of `phase` with `keep_alive` and `last_activity_ms` gives:

```
Connection = (ReadPhase + OwnedWrite + ViewWrite) × bool × int64_t
```

By the distributive law, this is isomorphic to:

```
(ReadPhase × bool × int64_t) + (OwnedWrite × bool × int64_t) + (ViewWrite × bool × int64_t)
```

Three variant alternatives, each with the shared fields attached. The factored form is more compact (the shared fields live in one place), so we use it. But the algebra guarantees that both representations carry the same information.

The key point: no buffer field sitting unused during a write, no cursor field sitting unused during a read. Each phase carries exactly what it needs. The dead fields are not just hidden — they do not exist.

### Nested Sums: WritePhase

The write phase is itself a sum:

```cpp
struct OwnedWrite {
    std::string data;
    size_t offset = 0;
};

struct ViewWrite {
    std::shared_ptr<const void> owner;
    const char* data;
    size_t len;
    size_t offset = 0;
};

using WritePhase = std::variant<OwnedWrite, ViewWrite>;
```

`OwnedWrite` owns its data (dynamically serialized response). `ViewWrite` borrows a pointer into the prebuilt cache (zero-copy from the hot reloader). The two alternatives carry different fields — `ViewWrite` needs an `owner` pointer (to keep the shared data alive) and an explicit length; `OwnedWrite` uses `std::string` which manages its own length.

A product type would carry all five fields with half of them meaningless in any given state. The `OwnedWrite` would have a dangling `owner` pointer and a `len` that contradicts `data.size()`. The `ViewWrite` would have an unused `std::string` occupying 32 bytes of stack space while the actual data lives somewhere else entirely. The product type is a hotel room where you reserved the whole floor but only sleep in one room — wasteful, and confusing to anyone who checks the reservation.

### HttpResponse: The Sum That Replaced a Flag

Loom's HTTP response was the motivating example for this entire algebraic approach. The old design was the quintessential "struct of lies":

```cpp
// Before: product with dead fields
struct Response {
    int status;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
    bool is_prebuilt;            // runtime tag
    std::shared_ptr<const void> owner;  // dead if !is_prebuilt
    const char* prebuilt_data;          // dead if !is_prebuilt
    size_t prebuilt_len;                // dead if !is_prebuilt
};
```

Seven fields. Three of them dead in approximately half of all use cases. One boolean playing the role of a variant tag without any of the type-system enforcement. Nothing prevents you from reading `prebuilt_data` when `is_prebuilt` is false. Nothing prevents you from accidentally setting `is_prebuilt` to true without populating the prebuilt fields. Nothing except your discipline, and discipline is a finite resource that depletes exactly when you need it most — at 3 AM during an incident.

The variant version eliminates every dead field:

```cpp
struct DynamicResponse {
    int status;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
};

struct PrebuiltResponse {
    std::shared_ptr<const void> owner;
    const char* data;
    size_t len;
};

using HttpResponse = std::variant<DynamicResponse, PrebuiltResponse>;
```

The `is_prebuilt` boolean is gone. The runtime tag check becomes structural pattern matching. The server's write path eliminates the variant with `std::visit` — owned responses are serialized and written from the string; prebuilt responses are written zero-copy directly from the cache. Each path sees exactly the fields it needs. The "dead fields" are not just avoided — they are structurally impossible.

Let us do the cardinality comparison. The old product type:

```
int × vec × string × bool × shared_ptr × char* × size_t
```

The bool doubles the state space. In half those states, three fields are meaningless. The product is too large by a factor of roughly `|shared_ptr| × |char*| × |size_t|` — a lot of nonsense states.

The variant type:

```
(int × vec × string) + (shared_ptr × char* × size_t)
```

Every value is meaningful. No dead fields. No contradictions. The sum is exactly the right size.

### ParseResult: Error Handling as a Sum

The HTTP parser returns success or failure as a variant:

```cpp
struct ParseError { std::string_view reason; };
using ParseResult = std::variant<HttpRequest, ParseError>;
```

This is `T + E` — the sum of success and failure. The caller must handle both cases; `std::visit` enforces exhaustiveness. Compare to the alternatives:

- Returning a bool with an out-parameter: `bool parse(string_view input, HttpRequest& out)`. Nothing prevents the caller from reading `out` when the function returned false. The success data is accessible in the failure state. The product type lies.
- Throwing an exception: `HttpRequest parse(string_view input)`. The failure mode is invisible in the signature. The caller does not know an exception is possible unless they read the documentation (they will not) or discover it in production (they will).
- Returning a magic value: `HttpRequest parse(string_view input)` where an empty request means failure. The error is hidden inside the success type, wearing a disguise. Good luck distinguishing "parse failed" from "the client sent an empty request."

The variant makes the disjunction explicit. The type signature says "this can succeed or fail, and here is what failure looks like." The compiler says "you must handle both cases." The algebra says "this is a sum type with cardinality |HttpRequest| + |ParseError|, and every inhabitant is meaningful." Everyone agrees. That is the power of making your types honest.

### The Overloaded Pattern

Loom defines the `overloaded` helper in `response.hpp`:

```cpp
template<typename... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<typename... Ts>
overloaded(Ts...) -> overloaded<Ts...>;
```

This single utility — two lines, about 100 characters — makes every `std::visit` in the codebase ergonomic. When the server writes a response:

```cpp
std::visit(overloaded{
    [&](const DynamicResponse& dr) { start_write_owned(fd, dr.serialize(keep_alive)); },
    [&](const PrebuiltResponse& pr) { start_write_view(fd, pr.owner, pr.data, pr.len); },
}, response);
```

Two cases. Two handlers. No fallthrough, no forgotten branch, no `default` that silently drops a case you did not think of. If a third response type were added, every `std::visit` in the codebase would fail to compile until updated. The elimination rule is enforced.

This is the compiler acting as your algebraic accountant. It counts the alternatives, counts your handlers, and refuses to sign off until the numbers match. Every time someone adds a variant alternative and sees the compiler light up with errors in twelve different files, that is the initial algebra theorem at work — the catamorphism must handle every case, and the compiler is checking your homework.

## Putting It All Together: A Design Checklist

Here is a practical checklist derived from everything above. The next time you define a data type, run through these questions:

1. **Count the inhabitants.** Multiply the cardinalities of the product fields. Is the number much larger than the set of meaningful values? If so, the type is too permissive.

2. **Look for dead fields.** Does any field become meaningless depending on the value of another field? If so, you have a product pretending to be a sum. Use a variant to eliminate the dead fields.

3. **Look for boolean flags.** Is a `bool` field serving as a variant tag? If so, distribute: the type `A × bool` is often better expressed as `variant<A_true_case, A_false_case>`, where each alternative carries only its relevant fields.

4. **Apply the distributive law.** If you have a struct with a variant field, consider distributing into a variant of structs. If you have a variant where alternatives share fields, consider factoring into a struct with a variant field. Pick whichever is more ergonomic.

5. **Validate at construction.** If the product type must satisfy invariants that the algebra cannot express, restrict construction with a private constructor and a factory function.

6. **Choose your axis of extension.** Is the set of alternatives fixed and the set of operations growing? Use a sum type. Is the set of operations fixed and the set of alternatives growing? Use a class hierarchy.

7. **Use the overloaded pattern.** If you are writing `std::visit` without it, you are suffering unnecessarily.

This is not dogma. Not every struct needs to be minimized. Not every bool needs to be a variant. The algebra is a tool for reasoning, not a religion. But when you spot a type whose cardinality is orders of magnitude larger than its meaningful state space, the algebra gives you both the diagnosis and the cure.

## Closing: Types Have Arithmetic

The arithmetic of types is not a curiosity. It is a design tool. It tells you when a struct is too permissive (the product is too large), when a variant is the right choice (you need a sum, not a product with a tag), and when two representations are interchangeable (the isomorphism is exact).

The semiring laws tell you which transformations are valid. The distributive law tells you when to factor. The initial algebra tells you that folding is the fundamental way to consume data. And the exponential law tells you why pattern matching is the natural elimination rule for sum types — it is not a design choice; it is an algebraic identity.

The next time you define a struct, count its inhabitants. Multiply the cardinalities of its fields. If that number is vastly larger than the set of values that actually make sense, your type is too permissive. Some of those extra inhabitants are bugs waiting to happen — they are the states your code will encounter in production, on a Friday, during a deploy, when the person who wrote the type is on vacation.

The fix is either to restrict construction (private constructors, factory functions — tightening the introduction rule) or to restructure the type (sums instead of products-with-tags — changing the algebraic expression). The algebra does not just describe the problem. It prescribes the solution.

Types have arithmetic. Learn the arithmetic, and the types will take care of themselves.

In the [next post](/post/pattern-matching), we dive deep into pattern matching — the elimination rule for algebraic types, exhaustiveness, nested patterns, and why a missing case is not a warning but a logical gap.
