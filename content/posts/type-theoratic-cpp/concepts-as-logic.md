---
title: "Concepts as Logic — Propositions, Proofs, and Predicates"
date: 2026-03-29
slug: concepts-as-logic
tags: [c++20, type-theory, concepts, logic, curry-howard]
excerpt: "C++20 concepts are more than template constraints. They are propositions about types. A requires-expression is a proof sketch. Concept composition follows the rules of propositional logic. This is the Curry-Howard correspondence, hiding in plain sight."
---

So far in this series we have used the type system to encode *values* (algebraic types), *identity* (phantom types), and *state* (typestate). Now we turn to a different question: what can we say *about* types themselves?

C++20 concepts let you name predicates over types. `Sortable`, `Hashable`, `Serializable` — these are statements about what a type can do. But viewed through a type-theoretic lens, concepts are something deeper. They are **propositions**. A concept is a statement. A type that satisfies a concept is a **witness** — evidence that the statement holds. And the rules for combining concepts — conjunction, disjunction, subsumption — follow the rules of propositional logic.

This post builds directly on [post #10 on concepts and constraints](/post/concepts-and-constraints) from the C++ series. We will use the same syntax but examine it through a different frame.

## A Concept Is a Predicate

At the simplest level, a concept is a compile-time predicate: a question you ask about a type that has a yes-or-no answer.

```cpp
template<typename T>
concept Printable = requires(T a, std::ostream& os) {
    { os << a };
};
```

This is the proposition: "For type T, there exists a way to stream a T value to an ostream." For `int`, this is true — the standard library defines `operator<<(ostream&, int)`. For a random user-defined struct with no streaming operator, it is false.

In logic, we might write: `Printable(T) ≡ ∃ (os << a)` for values of type T. The `requires` expression is the evidence — it shows *how* the predicate is satisfied.

## Requires-Expressions as Proof Sketches

A requires-expression does not execute code. It asks: "would this code compile?" The compiler checks the expression's well-formedness without running it.

```cpp
template<typename T>
concept Container = requires(T c) {
    { c.begin() } -> std::input_or_output_iterator;
    { c.end() } -> std::sentinel_for<decltype(c.begin())>;
    { c.size() } -> std::convertible_to<std::size_t>;
    typename T::value_type;
};
```

Each line in the requires-expression is a *proof obligation*: the type must provide `begin()` returning an iterator, `end()` returning a compatible sentinel, `size()` returning something convertible to `size_t`, and a nested `value_type` alias. If any line fails, the concept is not satisfied and any function constrained by it becomes invisible to overload resolution.

This is remarkably close to a formal proof in constructive logic. You don't prove a proposition by arguing abstractly — you prove it by *constructing* a witness. The requires-expression says: "here are the things that must be constructible," and the compiler checks each one.

## Logical Connectives

Concepts compose according to the rules of propositional logic:

### Conjunction (AND)

```cpp
template<typename T>
concept PrintableContainer = Container<T> && Printable<T>;
```

`PrintableContainer(T)` holds if and only if both `Container(T)` and `Printable(T)` hold. This is logical conjunction. A type must satisfy both predicates.

### Disjunction (OR)

```cpp
template<typename T>
concept StringLike = std::same_as<T, std::string>
                  || std::same_as<T, std::string_view>
                  || std::convertible_to<T, std::string_view>;
```

`StringLike(T)` holds if any of the three sub-propositions hold. A type need only satisfy one.

### Negation (NOT)

C++ does not have a built-in concept negation operator. You can write:

```cpp
template<typename T>
concept NotPointer = !std::is_pointer_v<T>;
```

But negation in concept subsumption is limited — the compiler does not fully reason about negated constraints. This is a deliberate design choice: negation introduces complexity in overload resolution.

### Implication

Concept subsumption gives us a form of implication. If concept `A` is defined in terms of concept `B`:

```cpp
template<typename T>
concept RandomAccessible = requires(T c, size_t i) {
    requires Container<T>;
    { c[i] } -> std::same_as<typename T::value_type&>;
};
```

Then `RandomAccessible(T) → Container(T)` — anything that is random-accessible is necessarily a container. The compiler uses this for overload resolution: a function constrained by `RandomAccessible` is *more specific* than one constrained by `Container`, so it wins when both match.

## Subsumption: The Compiler's Logic Engine

Concept subsumption is where the logic gets interesting. The compiler maintains a partial order on constraints and uses it to select the most specific matching overload:

```cpp
template<typename T>
concept Integral = std::is_integral_v<T>;

template<typename T>
concept SignedIntegral = Integral<T> && std::is_signed_v<T>;

template<Integral T>
auto absolute(T value) -> T {
    return value < 0 ? -value : value;
}

template<SignedIntegral T>
auto absolute(T value) -> T {
    return value < 0 ? -value : value;
}
```

For `unsigned int`: only the first overload matches (it satisfies `Integral` but not `SignedIntegral`). For `int`: both match, but the compiler selects the second because `SignedIntegral` *subsumes* `Integral` — it is a strictly stronger proposition.

This is modus ponens in action. The compiler reasons: `SignedIntegral(T) → Integral(T)`, and since both are satisfied, the more specific (stronger) constraint wins. No ambiguity, no `enable_if` trickery, no SFINAE. The logic is structural.

## Universal and Existential Quantification

Templates give us universal quantification. A function template:

```cpp
template<Container T>
auto count(const T& c) -> size_t {
    return c.size();
}
```

says: "for all types T that satisfy `Container`, this function exists." This is ∀T. Container(T) → (count : T → size_t). The concept is the premise, the function is the conclusion. The template instantiation is the proof: at each call site, the compiler verifies the premise and constructs the function.

Existential quantification — "there exists a T such that..." — is trickier in C++. The closest approximation is type erasure:

```cpp
// "There exists some type that satisfies Container"
class AnyContainer {
    struct Concept {
        virtual auto size() const -> size_t = 0;
        virtual ~Concept() = default;
    };
    template<Container T>
    struct Model : Concept {
        T value;
        auto size() const -> size_t override { return value.size(); }
    };
    std::unique_ptr<Concept> impl_;
public:
    template<Container T>
    AnyContainer(T value) : impl_(std::make_unique<Model<T>>(std::move(value))) {}
    auto size() const -> size_t { return impl_->size(); }
};
```

`AnyContainer` says: "I hold *some* type that satisfies `Container`, but I'm not telling you which one." This is existential: ∃T. Container(T). The concrete type is hidden behind the virtual dispatch — it exists, it satisfies the concept, but it has been *forgotten* (erased).

## Concepts as Interfaces Without Inheritance

Inheritance in C++ creates *nominal* subtyping: `class Dog : public Animal` explicitly declares the relationship. Concepts create *structural* subtyping: any type that provides the right operations satisfies the concept, without declaring anything.

```cpp
template<typename T>
concept Drawable = requires(T d, Canvas& c) {
    { d.draw(c) } -> std::same_as<void>;
    { d.bounds() } -> std::same_as<Rect>;
};
```

Any type with `draw(Canvas&)` and `bounds()` methods satisfies `Drawable` — whether it is a `Circle`, a `TextBox`, a third-party `Widget`, or a `MockShape` in a test. No base class needed. No vtable. No modification of the satisfying type.

This is the **open world assumption** from logic: new types can satisfy the concept at any time, even types that were written before the concept existed. Inheritance is the closed world — you must modify the class to make it derive from the base. Concepts leave the world open.

## Concept Composition Patterns

Several compositional patterns emerge from treating concepts as logic:

### Refined Concepts (Specialization)

Build stronger concepts from weaker ones, each adding new requirements:

```cpp
template<typename T>
concept Readable = requires(T r) {
    { r.read() } -> std::convertible_to<std::span<const char>>;
};

template<typename T>
concept Seekable = Readable<T> && requires(T r, size_t pos) {
    { r.seek(pos) };
    { r.position() } -> std::convertible_to<size_t>;
};

template<typename T>
concept BufferedReadable = Readable<T> && requires(T r, size_t n) {
    { r.peek(n) } -> std::convertible_to<std::span<const char>>;
    { r.buffer_size() } -> std::convertible_to<size_t>;
};
```

Each concept adds requirements. `Seekable` is a `Readable` that can also seek. `BufferedReadable` is a `Readable` with a look-ahead buffer. Functions constrained by `Seekable` automatically know their argument is `Readable`.

### Orthogonal Concepts (Intersection)

Combine independent capabilities:

```cpp
template<typename T>
concept Serializable = requires(T a) {
    { serialize(a) } -> std::same_as<std::string>;
};

template<typename T>
concept Loggable = Printable<T> && requires(T a) {
    { a.log_level() } -> std::same_as<LogLevel>;
};

// A function that needs both:
template<typename T>
    requires Serializable<T> && Loggable<T>
void persist_and_log(const T& value);
```

The concepts are orthogonal — neither implies the other. A type can be serializable without being loggable, or vice versa. The function requires both, and the constraint is the conjunction.

### Concept Maps (Adapters)

Sometimes a type almost satisfies a concept but uses different names. You can write an adapter:

```cpp
// Third-party type uses 'length()' instead of 'size()'
struct ExternalBuffer {
    auto length() const -> size_t;
    auto data() const -> const char*;
};

// Adapter satisfies Container-like concepts
struct BufferAdapter {
    ExternalBuffer& buf;
    auto size() const -> size_t { return buf.length(); }
    auto data() const -> const char* { return buf.data(); }
};
```

This is not as elegant as Haskell's type class instances, but it serves the same purpose: making a type conform to a concept without modifying the type.

## The Curry-Howard Bridge

We can now state the correspondence explicitly:

| Logic | C++ |
|---|---|
| Proposition | Concept (or type) |
| Proof | Satisfying type (or value) |
| Conjunction (A ∧ B) | `Concept1<T> && Concept2<T>` |
| Disjunction (A ∨ B) | `Concept1<T> \|\| Concept2<T>` |
| Implication (A → B) | Concept subsumption |
| Universal (∀T. P(T)) | Template with concept constraint |
| Existential (∃T. P(T)) | Type erasure |
| Modus ponens | Overload resolution via subsumption |

C++ is not a proof assistant. The correspondence is imperfect — there is no way to prove arbitrary theorems, no termination checking, no universe hierarchy. But the fragment that C++ does support is exactly the fragment that matters for practical software engineering: stating requirements, checking them at compile time, and selecting the most specific implementation.

## Concepts in Loom

Loom uses concepts sparingly but precisely. The [strong types](/post/strong-types) system uses concepts to conditionally enable operations:

```cpp
template<typename Tag, typename T>
struct StrongType {
    T value;

    auto operator==(const StrongType& other) const -> bool
        requires std::equality_comparable<T>
    { return value == other.value; }

    auto str() const -> const std::string&
        requires std::same_as<T, std::string>
    { return value; }
};
```

The `requires` clause is a proposition: "this method exists *if and only if* the underlying type satisfies this property." For `StrongType<SlugTag, std::string>`, `str()` exists because strings are strings. For a hypothetical `StrongType<CountTag, int>`, `str()` does not exist — calling it would be a compile error.

The [compile-time router](/post/router) uses concepts for pattern analysis:

```cpp
template<typename T>
concept StaticRoute = (T::pattern.find(':') == std::string_view::npos);
```

This is a proposition about a route's compile-time string: "the pattern contains no parameter markers." The compiler evaluates this at template instantiation and selects the appropriate matching strategy — exact string comparison for static routes, prefix+parameter extraction for dynamic routes.

## Closing: A Logic You Already Use

You have been doing logic every time you write a concept, constrain a template, or select between overloads. The vocabulary of propositions, proofs, and connectives is not foreign to C++ — it is already embedded in the language. Naming it makes it deliberate.

The practical consequence: treat concept design like API design. Concepts are the contract between generic code and its users. A well-designed concept is minimal (requires only what is needed), composable (can be combined with other concepts), and subsumable (fits into a refinement hierarchy). A poorly-designed concept is either too broad (admits types that should not qualify) or too narrow (excludes types that should).

In the [next post](/post/compile-time-data), we explore what happens when *values* enter the type system — when non-type template parameters blur the line between data and types, and the compiler starts computing with values at compile time.
