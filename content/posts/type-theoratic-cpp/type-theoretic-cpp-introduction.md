---
title: "A Program Is a Theory — Foundations of Type-Theoretic C++"
date: 2026-03-29
slug: type-theoretic-foundations
tags: [c++20, type-theory, compile-time, design, foundations]
excerpt: "Type judgments, formation rules, Curry-Howard, and the lambda cube — real type theory, made concrete in modern C++. A program is a formal system, and the compiler is its first reviewer."
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

## What *Is* a Type System?

Before we use the C++ type system to do interesting things, we should ask what a type system actually *is*. Not what it does — what it *is*, formally.

A type system is a set of rules for assigning types to expressions. That sounds tautological. It is not. The key word is *rules*. A type system is a *logic* — a deductive system where you derive conclusions (this expression has this type) from premises (these variables have these types) using inference rules.

In type theory, we write these derivations using a notation called **type judgments**:

```
Γ ⊢ e : τ
```

Read this as: "Under the typing context Γ, expression e has type τ."

The context Γ is a set of assumptions — the types of all variables currently in scope. When you write:

```cpp
auto process(int x, std::string name) -> bool {
    // ...
}
```

Inside the body of `process`, the typing context is `Γ = {x : int, name : std::string}`. Every expression you write inside that body is checked against this context. If you write `x + 1`, the compiler derives:

```
{x : int} ⊢ x + 1 : int
```

"Given that x is an int, the expression x + 1 has type int." That is a type judgment. The compiler derives it by applying rules — it knows `int + int → int`, it knows `1 : int`, and it looks up `x` in the context to confirm `x : int`.

This is not a metaphor. The C++ compiler literally maintains a typing context (the symbol table) and applies inference rules (overload resolution, template deduction, implicit conversions) to derive the type of every expression. When it fails — when no rule applies — you get a type error. A type error is a failed derivation.

### Why This Notation Matters

You might think: "I know the compiler checks types, I don't need Greek letters." But the notation reveals something important: **the context is explicit**. A type is not an intrinsic property of an expression — it is a *judgment relative to a context*. The expression `x` has no type on its own. It has a type only because the context says `x : int`.

This matters because it explains why the same expression can have different types in different places:

```cpp
template<typename T>
auto identity(T x) -> T {
    return x;  // x : T, where T is whatever the caller provides
}
```

Inside `identity`, the context is `{T : Type, x : T}`. The variable `x` has type `T`, which is not yet known — it is a *parameter* of the judgment. When you call `identity(42)`, the compiler *instantiates* the judgment with `T = int`, producing `{x : int} ⊢ x : int`. When you call `identity("hello"s)`, it produces `{x : std::string} ⊢ x : std::string`. Same code, different types, different contexts.

This is the foundation of generic programming. A template is a *family* of type judgments, parameterized over the context.

## Formation, Introduction, and Elimination

Every type in type theory has three kinds of rules. Understanding these three rules for a type tells you *everything* about how that type works. This is perhaps the most useful idea in all of type theory for a working programmer.

**Formation rules** tell you when the type itself is valid. You cannot write `std::optional<void>` (it is not a valid type). You *can* write `std::optional<int>`. The formation rule for `optional<T>` is: if `T` is a valid type and not a reference and not `void` and not `std::in_place_t`, then `optional<T>` is a valid type.

**Introduction rules** tell you how to *create* values of the type. These are the constructors — not in the C++ class sense, but in the logical sense: the ways to introduce evidence that the type is inhabited.

**Elimination rules** tell you how to *use* values of the type. These are the ways to extract information — pattern matching, member access, destructuring.

Let us trace this through several C++ types:

### `std::optional<T>`

```
Formation:     T is a type  →  optional<T> is a type
Introduction:  nullopt      →  optional<T>         (the "nothing" case)
               value v : T  →  optional<T>         (the "something" case)
Elimination:   opt.has_value() ? use opt.value() : handle_empty()
```

In C++:

```cpp
// Introduction — two ways to create an optional
std::optional<int> a = std::nullopt;          // introduce "nothing"
std::optional<int> b = 42;                     // introduce "something"

// Elimination — the only safe way to use an optional
if (b.has_value()) {
    int x = b.value();  // extract the value
}
```

The elimination rule is: you must check which introduction rule was used (has_value or not) before you can access the contained value. If you skip the check — if you call `.value()` on an empty optional — you violate the elimination rule, and the result is an exception. The type system does not enforce this particular elimination rule statically in standard C++, which is a weakness.

### `std::variant<A, B, C>`

```
Formation:     A, B, C are types  →  variant<A, B, C> is a type
Introduction:  a : A  →  variant<A, B, C>     (inject A)
               b : B  →  variant<A, B, C>     (inject B)
               c : C  →  variant<A, B, C>     (inject C)
Elimination:   std::visit with exhaustive handler
```

```cpp
// Introduction — one value per alternative
std::variant<int, std::string, double> v = 42;

// Elimination — must handle ALL alternatives
std::visit(overloaded{
    [](int i)                { /* ... */ },
    [](const std::string& s) { /* ... */ },
    [](double d)             { /* ... */ },
}, v);
```

The elimination rule for variant is *exhaustive visitation*: you must provide a handler for every possible introduction form. If you forget one, the compiler tells you. This is the variant's great strength — the elimination rule is enforced at compile time.

### `std::pair<A, B>` (Product type)

```
Formation:     A, B are types  →  pair<A, B> is a type
Introduction:  a : A, b : B   →  pair<A, B>   (combine both)
Elimination:   p.first : A                     (project left)
               p.second : B                    (project right)
```

A product type has one introduction form (provide both components) and two elimination forms (extract either component). This is the logical AND — to prove "A and B", you need evidence of both. To use "A and B", you can extract evidence of either.

### The Pattern

Once you see this three-part structure, you see it everywhere. A `unique_ptr<T>` is introduced by `make_unique` and eliminated by `operator*` or `operator->`. A function type `A → B` is introduced by defining a lambda and eliminated by calling it. An enum is introduced by naming an enumerator and eliminated by switching on it.

The deep insight: **the introduction and elimination rules of a type determine its computational meaning**. They define what you can *do* — how you create values and how you take them apart. Everything else is derived.

## The Curry-Howard Correspondence

Here is the idea that connects programming to logic, that turns code into proof and proof into code. It deserves more than a table.

The Curry-Howard correspondence says: **types are propositions, and values are proofs**. A type is a logical statement. A value of that type is evidence that the statement is true. A function from type A to type B is a proof that A implies B — you give me evidence of A, and I produce evidence of B.

This is not a metaphor. It is a precise mathematical correspondence discovered independently by Haskell Curry (1934) and William Howard (1969).

### Functions as Implications

Consider a function:

```cpp
auto first_char(NonEmptyString s) -> char {
    return s.get()[0];
}
```

Read this through Curry-Howard:

- `NonEmptyString` is the proposition "this string is non-empty"
- `char` is the proposition "a character exists"
- The function type `NonEmptyString → char` is the implication "if the string is non-empty, then a character exists"
- The function *body* is the *proof* of that implication — it constructs the character by extracting the first element

The function is not just computing a value. It is *proving a theorem*: non-empty strings have first characters. And the compiler verifies the proof by type-checking the body.

### Products as Conjunction

A `struct` with fields A and B is a proof of "A and B":

```cpp
struct Evidence {
    NonEmptyString name;     // proof that name is non-empty
    PositiveInt age;         // proof that age is positive
};
```

Holding an `Evidence` value means you have *both* proofs simultaneously. To construct it, you must provide both. This is logical AND — introduction requires both conjuncts, elimination gives you either one.

### Sums as Disjunction

A `std::variant<A, B>` is a proof of "A or B":

```cpp
using ParseResult = std::variant<ParsedValue, ParseError>;
```

Holding a `ParseResult` means you have a proof of *either* successful parsing *or* an error — but you do not know which until you inspect it. This is logical OR — introduction requires one disjunct, elimination requires handling both cases.

### Void as Falsity

The empty type — a type with no values — is the proposition **false**. There is no evidence for it. In C++, `std::variant<>` (a variant with no alternatives) is truly uninhabitable. The more pragmatic stand-in is `void` (which is not quite the same, but directionally correct).

A function with return type `void` is suggestive: it says "I do not produce evidence." A function that *takes* an uninhabitable type can never be called — there is no way to construct the argument. This corresponds to the logical principle *ex falso quodlibet*: from false, anything follows.

### Unit as Trivial Truth

`std::monostate` (or any empty struct) has exactly one value. It is the proposition **true** — trivially satisfied, carrying no information. It is the multiplicative identity: `A × Unit ≅ A`. Having a unit value tells you nothing you did not already know.

### The Profound Consequence

The Curry-Howard correspondence means that type checking *is* proof checking. When the compiler accepts your program, it has verified that your proofs are valid — that your evidence supports your claims. When it rejects your program, it has found a flaw in your reasoning.

This reframes type errors from "the compiler is being pedantic" to "your proof is invalid." The compiler is not an obstacle. It is a theorem prover operating on your code.

## The Two Languages Inside C++

C++ is not one language. It is two, running at different times, speaking different dialects, but sharing the same syntax.

**The type language** runs at compile time. It describes what exists, what is valid, what is distinguishable, and what is impossible. Its vocabulary includes types, templates, concepts, constant expressions, and type relationships. It is declarative: you state what must be true, and the compiler either confirms it or rejects the program.

**The value language** runs at runtime. It describes what happens — how values flow, how effects occur, how the program interacts with external reality. It is imperative: you describe steps, and the CPU executes them.

In type theory, this separation is called the **phase distinction**. The compile-time phase reasons about *structure* — what kinds of things exist and how they relate. The runtime phase reasons about *computation* — what happens when structures interact with data.

Most codebases over-invest in the value language and under-invest in the type language. They write elaborate runtime validation — null checks, range checks, state flags, assertion macros — because their types are too weak to rule anything out. Every function defends itself against the universe because the type system has made no promises about what can arrive.

Type-theoretic C++ flips that balance. We push as much reasoning as possible into compile time, so that by the time a function runs, the only things that reach it are things that *should* reach it.

## The Lambda Cube: Where C++ Lives

Type theory organizes languages along three axes of expressiveness, forming what Henk Barendregt called the **lambda cube**. Each axis represents a dependency between terms (values) and types:

**Axis 1: Terms depending on types** — This is parametric polymorphism. A function that works for any type: `template<typename T> auto id(T x) -> T`. The term (the function body) depends on a type parameter. C++ has this via templates.

**Axis 2: Types depending on types** — This is type constructors. `std::vector<T>` takes a type and produces a type. `template<template<typename> typename Container>` is a type that depends on another type constructor. C++ has this via template template parameters.

**Axis 3: Types depending on terms** — This is dependent types. A type whose definition depends on a runtime value: `Vec<int, n>` where `n` is a number. C++ *approximates* this with non-type template parameters (`template<int N> struct Vec`), but only for compile-time constants. True dependent types allow `n` to be a runtime value.

The corners of the cube give us named systems:

| System | Terms→Types | Types→Types | Types→Terms |
|---|---|---|---|
| Simply Typed (λ→) | ✗ | ✗ | ✗ |
| System F (λ2) | ✓ | ✗ | ✗ |
| System Fω (λω) | ✓ | ✓ | ✗ |
| Calculus of Constructions (λC) | ✓ | ✓ | ✓ |

C++ sits somewhere between System Fω and the Calculus of Constructions. It has full polymorphism (templates), full type constructors (template template parameters), and a restricted form of dependent types (NTTPs must be `constexpr`). It does not have full dependent types — you cannot write `Vec<int, n>` where `n` is computed at runtime. This restriction keeps type checking decidable.

We will explore what C++ can and cannot do along each axis throughout this series, especially in [part 6](/post/compile-time-data) on compile-time data.

## What a Type System Is *Not* — Soundness and Its Limits

A sound type system makes a promise: **well-typed programs do not go wrong**. More precisely, if the type checker accepts a program, the program will not exhibit undefined behavior at runtime (for the class of errors the type system tracks).

C++ is **not sound**.

This is an important and honest admission. `reinterpret_cast` lets you lie about types. Dangling references violate type assumptions. Buffer overflows access memory the type system said nothing about. Undefined behavior means the compiler's reasoning can be invalidated by the very code it compiled.

But unsoundness is not the same as uselessness. A lock on a door is not useless because windows exist. The C++ type system catches vast classes of errors — argument mismatches, missing return values, incompatible conversions, incomplete variant handling, concept violations. The fact that you *can* circumvent it (with casts, UB, or raw pointers) does not diminish the value of what it catches when you stay within its rules.

Type-theoretic C++ is about maximizing the territory where the type system *does* protect you — designing your code so that the unsafe escape hatches are never needed in normal operation. The boundary between "well-typed" and "trust me" should be explicit, narrow, and pushed to the edges of the system.

## Illegal States Are Logical Contradictions

The popular advice "make invalid states unrepresentable" is usually presented as a pragmatic design tip. It is deeper than that. In type-theoretic terms:

> Making invalid states unrepresentable means making logical contradictions unconstructible.

Consider:

```cpp
struct File {
    int fd;
    bool open;
};
```

This type permits four combinations. Two make sense. Two are contradictions — an open file with no descriptor, a closed file holding a live descriptor. The type permits contradictions, so every function must defend against them.

```cpp
struct OpenFile {
    int fd;  // guaranteed valid
    auto read(std::span<char> buf) -> ssize_t;
    auto close() -> ClosedFile;
};

struct ClosedFile {};  // no fd — nothing to leak
```

The contradictions are gone. We did not add validation. We removed the need for it by making the types faithful to the domain. In terms of formation rules, we restricted the introduction forms until only valid states could be constructed.

## The Design Discipline

We arrive at the core methodology. When facing a new design problem, we ask a sequence of type-theoretic questions:

**What exists?** What are the fundamental entities? Not the operations — the *things*. Users, connections, tokens, documents, routes.

**What are the formation rules?** When is each type valid? What preconditions must hold for a value to be constructible?

**What are the introduction rules?** How are values created? What are the constructors, and what evidence must they receive?

**What are the elimination rules?** How are values used? What must the consumer check or handle?

**What is impossible?** What states must never be representable? These are the contradictions we want the type system to rule out.

**What can be decided at compile time?** What depends only on the program's structure? Route matching, configuration schemas, protocol shapes, unit conversions.

**What remains for runtime?** Only what genuinely depends on the external world: user input, network responses, file contents, wall-clock time.

## A Taste of What Is Coming

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

Read this through our new vocabulary:

- **Formation**: `Email` is a valid type (always).
- **Introduction**: Only via `validate()`. The private constructor enforces this — there is exactly one introduction rule, and it requires evidence (a string that passes validation).
- **Elimination**: `get()` extracts the string. No check needed — the introduction rule guaranteed validity.

The `validate` function is, through Curry-Howard, a *proof procedure*: given a raw string, it either produces evidence of validity (an `Email`) or fails (returns `nullopt`). Every function downstream that accepts `Email` is stating a precondition and receiving a proof that it holds.

This pattern — *parse, don't validate* — is one of the central techniques of type-theoretic design.

## The Road Ahead

This is the first post in a ten-part series. We are building toward real systems where protocols are encoded in types, state machines are enforced structurally, APIs are impossible to misuse, and all of it compiles down to the same machine code as the hand-written alternative.

The series will cover:

* **[Sum Types and Product Types](/post/algebraic-data-types)** — the algebra of C++ types, initial algebras, catamorphisms, and why every domain model is a polynomial expression
* **[Phantom Types](/post/phantom-types)** — zero-cost type distinctions, the newtype pattern, parametricity, and representation independence
* **[Typestate Programming](/post/typestate-programming)** — encoding state machines in the type system, linear logic, and protocols that cannot be violated
* **[Concepts as Logic](/post/concepts-as-logic)** — natural deduction, intuitionistic logic, and the Curry-Howard bridge in C++20
* **[Compile-Time Data](/post/compile-time-data)** — dependent types, Pi and Sigma types, the lambda cube, and the compiler as a staged computation engine
* **[Parametricity](/post/parametricity)** — free theorems, Reynolds' abstraction theorem, and what type signatures alone can prove
* **[Substructural Types](/post/substructural-types)** — linear logic, affine types, resource management, and why RAII is deeper than you think
* **[Recursive Types and Fixed Points](/post/recursive-types-and-fixed-points)** — mu types, F-algebras, catamorphisms, and the algebra of data
* **[Building a Type-Safe Protocol](/post/type-safe-protocol)** — putting every technique together into a real, compile-time verified protocol stack

Each post will include real C++ code, real design patterns, and the type-theoretic ideas that motivate them.

## Closing Thought

Most bugs are not subtle. They come from mixing things that should not mix, calling functions at the wrong time, forgetting to check an invariant, or representing a domain too loosely. All of these are failures of *representation* — the types permitted something that the domain forbids.

The fix is not more defensive code. It is better types. Better formation rules. Tighter introduction forms. Exhaustive elimination.

The goal is not "write better functions." The goal is **design a world where bad functions cannot exist.** Once that world is built — once the types faithfully represent the domain — everything else becomes simpler. Functions get shorter. Tests get fewer. Edge cases vanish. And the compiler, that strict, pedantic, slightly judgmental theorem prover, becomes your most reliable collaborator.

Let's build that world.
