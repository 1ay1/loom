---
title: "Concepts as Logic — Propositions, Proofs, and Predicates"
date: 2026-03-25
slug: concepts-as-logic
tags: [c++20, type-theory, concepts, logic, curry-howard, natural-deduction]
excerpt: "C++20 concepts are propositions in intuitionistic logic. Requires-expressions are constructive proofs. Subsumption is modus ponens. This is natural deduction, hiding in your compiler."
---

So far in this series we have used the type system to encode *values* ([algebraic types](/post/algebraic-data-types)), *identity* ([phantom types](/post/phantom-types)), and *state* ([typestate](/post/typestate-programming)). Now we turn to a different question: what can we say *about* types themselves?

C++20 concepts let you name predicates over types. But viewed through a type-theoretic lens, concepts are something deeper. They are **propositions** in a formal logic — specifically, in **intuitionistic logic**. A type that satisfies a concept is a **witness**. The rules for combining concepts follow **natural deduction**. And the compiler's overload resolution implements **modus ponens**.

This post builds on [post #10 on concepts and constraints](/post/concepts-and-constraints) from the C++ series. Same syntax, different frame.

## Natural Deduction: The Framework

To understand concepts as logic, we need the framework of **natural deduction**, introduced by Gerhard Gentzen in 1935. Natural deduction has two kinds of rules for each logical connective:

- **Introduction rules**: how to *prove* a proposition
- **Elimination rules**: how to *use* a proof

This mirrors exactly the formation/introduction/elimination pattern from [part 1](/post/type-theoretic-foundations). Logic and type theory share the same structure — that is the Curry-Howard correspondence.

## A Concept Is a Proposition

At the simplest level, a concept is a compile-time predicate:

```cpp
template<typename T>
concept Printable = requires(T a, std::ostream& os) {
    { os << a };
};
```

This is the proposition: "For type T, there exists a way to stream T to an ostream." For `int`, this is true. For a random struct with no `operator<<`, it is false.

### Requires-Expressions as Constructive Proofs

A requires-expression does not execute code. It asks: "would this code compile?" The compiler checks well-formedness without running anything.

```cpp
template<typename T>
concept Container = requires(T c) {
    { c.begin() } -> std::input_or_output_iterator;
    { c.end() } -> std::sentinel_for<decltype(c.begin())>;
    { c.size() } -> std::convertible_to<std::size_t>;
    typename T::value_type;
};
```

Each line is a **proof obligation**: the type must provide `begin()` returning an iterator, `end()` returning a compatible sentinel, `size()` returning something convertible to `size_t`, and a nested `value_type`.

This is remarkably close to a proof in constructive logic. You do not prove a proposition by arguing abstractly — you prove it by *constructing* a witness. The requires-expression says: "here are the things that must be constructible." The compiler checks each one.

## Intuitionistic Logic: Why It Matters

C++ concepts live in **intuitionistic logic**, not classical logic. This is a crucial distinction.

In **classical logic**, the law of excluded middle holds: `A ∨ ¬A` is always true. Either a proposition is true or its negation is true. Double negation elimination holds: `¬¬A → A`.

In **intuitionistic logic**, these do not hold. To prove `A ∨ B`, you must *construct* a proof of A or a proof of B — you cannot just say "one of them must be true." To prove A, you must construct it directly — knowing that `¬A` leads to contradiction is not enough.

Why does this matter for C++? Because concept satisfaction is *constructive*. The compiler does not reason "either T satisfies Container or it doesn't" — it *checks*. It attempts to construct the proof (verify each requires-clause). If construction succeeds, the concept is satisfied. If it fails, the concept is not satisfied. There is no middle ground.

The practical consequence: **concept negation is limited**. You can write:

```cpp
template<typename T>
concept NotPointer = !std::is_pointer_v<T>;
```

But `!(!Concept<T>)` is NOT the same as `Concept<T>` in the compiler's subsumption logic. Double negation elimination fails. This is not a compiler bug — it is a faithful implementation of intuitionistic logic.

## The BHK Interpretation

The **Brouwer-Heyting-Kolmogorov interpretation** gives constructive meaning to each logical connective:

- A proof of **A ∧ B** is a pair: (proof of A, proof of B)
- A proof of **A ∨ B** is a tagged value: either (left, proof of A) or (right, proof of B)
- A proof of **A → B** is a function: given a proof of A, produce a proof of B
- There is **no proof of ⊥** (falsity)
- A proof of **¬A** is a function from A to ⊥ (a proof that A leads to contradiction)

This IS the Curry-Howard correspondence:

| BHK (Logic) | Type Theory | C++ |
|---|---|---|
| Proof of A ∧ B | Pair (A, B) | `std::pair<A, B>`, struct |
| Proof of A ∨ B | Tagged union | `std::variant<A, B>` |
| Proof of A → B | Function A → B | `auto f(A) -> B` |
| Proof of ⊤ (truth) | Unit value | `std::monostate{}` |
| Proof of ⊥ (falsity) | — (impossible) | `std::variant<>` (uninhabitable) |
| Proof of ¬A | Function A → ⊥ | `auto f(A) -> void` (approximation) |
| Proof of ∀x. P(x) | Generic function | `template<Concept T> auto f(T)` |
| Proof of ∃x. P(x) | Existential package | Type erasure |

## Logical Connectives in Concepts

### Conjunction (AND) — Introduction and Elimination

```cpp
template<typename T>
concept PrintableContainer = Container<T> && Printable<T>;
```

**Introduction**: to prove `PrintableContainer<T>`, you need proofs of both `Container<T>` and `Printable<T>`. In natural deduction:

```
  Container<T>    Printable<T>
  ─────────────────────────────  (∧-intro)
    PrintableContainer<T>
```

**Elimination**: from `PrintableContainer<T>`, you can derive either conjunct. A function constrained by `PrintableContainer<T>` can use T as both a container and a printable type.

### Disjunction (OR) — Introduction and Elimination

```cpp
template<typename T>
concept StringLike = std::same_as<T, std::string>
                  || std::same_as<T, std::string_view>
                  || std::convertible_to<T, std::string_view>;
```

**Introduction**: proving any disjunct proves the disjunction. `std::string` satisfies `StringLike` because it satisfies the first alternative.

**Elimination**: you must handle all cases. In practice, concept disjunction is used for overload sets where any matching branch suffices.

### Implication — Subsumption

Concept subsumption gives us implication:

```cpp
template<typename T>
concept Integral = std::is_integral_v<T>;

template<typename T>
concept SignedIntegral = Integral<T> && std::is_signed_v<T>;
```

`SignedIntegral<T> → Integral<T>` — anything signed-integral is integral. The compiler uses this for overload resolution:

```cpp
template<Integral T>
auto absolute(T value) -> T { return value < 0 ? -value : value; }

template<SignedIntegral T>
auto absolute(T value) -> T { return value < 0 ? -value : value; }
```

For `int`: both match, but `SignedIntegral` *subsumes* `Integral` — it is a strictly stronger proposition. The compiler selects the more specific overload.

This is **modus ponens**: given `SignedIntegral<T> → Integral<T>` and a proof that T is `SignedIntegral`, the compiler derives that T is `Integral`. Since the specific overload is more constraining, it wins. No ambiguity, no SFINAE. The logic is structural.

## Universal and Existential Quantification

### Universal (∀): Templates

A constrained template is universal quantification:

```cpp
template<Container T>
auto count(const T& c) -> size_t {
    return c.size();
}
```

This says: ∀T. Container(T) → (count : T → size_t). "For all types T satisfying Container, this function exists." The template instantiation is the proof: at each call site, the compiler verifies the premise and constructs the function.

### Existential (∃): Type Erasure

Existential quantification — "there exists a T such that..." — uses type erasure:

```cpp
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

`AnyContainer` says: ∃T. Container(T) ∧ (I hold a T). The concrete type is hidden behind virtual dispatch. It exists, it satisfies the concept, but it has been *erased*. This is the existential package from type theory.

## Proof Irrelevance

In C++, concept satisfaction is **proof-irrelevant**: it does not matter *how* a type satisfies a concept, only *that* it does. If `int` supports `operator<` through implicit conversion and also through a direct overload, `std::totally_ordered<int>` is just `true` — the specific mechanism is invisible to the concept.

This is a design choice. In dependently typed languages (Agda, Coq), proofs can be *relevant* — different proofs of the same proposition are distinguishable and carry different computational content. Proof irrelevance simplifies concept checking (it is just a boolean) but prevents concepts from carrying information about *how* a type satisfies them.

## Open vs Closed World

**Concepts** assume an **open world**: new types can satisfy existing concepts at any time. You can define a concept `Drawable`, and a year later someone writes a type that satisfies it, without modifying any existing code.

**Inheritance** assumes a **closed world**: to make a type satisfy an interface, you must modify the type definition to derive from the base class.

In logical terms: the open world allows new axioms to be added. The closed world fixes the axioms upfront. Concepts are more flexible — they support retroactive conformance (a type satisfies a concept without declaring it). The trade-off: you cannot enumerate all types that satisfy a concept (the set is open), while you *can* enumerate all subclasses of a base class (the set is closed in the translation unit).

## Concept Composition Patterns

### Refined Concepts (Specialization)

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
```

Each concept adds requirements. `Seekable` subsumes `Readable`. This forms a partial order — a lattice of concepts from weakest to strongest.

### Orthogonal Concepts (Intersection)

```cpp
template<typename T>
concept Serializable = requires(T a) {
    { serialize(a) } -> std::same_as<std::string>;
};

template<typename T>
    requires Serializable<T> && Printable<T>
void persist_and_log(const T& value);
```

Independent capabilities combined by conjunction.

### Concept Maps (Adapters)

When a type almost satisfies a concept but uses different names:

```cpp
struct ExternalBuffer {
    auto length() const -> size_t;  // not 'size()'
    auto data() const -> const char*;
};

struct BufferAdapter {
    ExternalBuffer& buf;
    auto size() const -> size_t { return buf.length(); }
    auto data() const -> const char* { return buf.data(); }
};
```

Not as elegant as Haskell's type class instances, but serves the same purpose: retroactive conformance via wrapping.

## The Full Curry-Howard Bridge

| Logic | Type Theory | C++ |
|---|---|---|
| Proposition | Type / Concept | `concept C = ...` |
| Proof | Value / Satisfying type | `int` satisfies `Integral` |
| Conjunction (A ∧ B) | Product type | `C1<T> && C2<T>` |
| Disjunction (A ∨ B) | Sum type | `C1<T> \|\| C2<T>` |
| Implication (A → B) | Function type | Concept subsumption |
| Universal (∀T. P(T)) | Parametric type | `template<Concept T>` |
| Existential (∃T. P(T)) | Existential type | Type erasure |
| Modus ponens | Function application | Overload resolution |
| ∧-introduction | Pair construction | Satisfying both concepts |
| ∧-elimination | Projection | Using either capability |
| ∨-introduction | Injection | Satisfying one alternative |
| ∨-elimination | Pattern matching | `std::visit` / overload selection |

C++ is not a proof assistant. The correspondence is imperfect — there is no termination checking, no universe hierarchy, no proof terms. But the fragment C++ supports is exactly the fragment that matters for practical engineering: stating requirements, checking them at compile time, and selecting the most specific implementation.

## In Loom

Loom uses concepts at three levels: defining capabilities for subsystems, constraining generic components, and expressing conditional method formation.

### ContentSource: A Proposition About Data Providers

```cpp
template<typename T>
concept ContentSource = requires(T source) {
    { source.all_posts() } -> std::same_as<std::vector<Post>>;
    { source.all_pages() } -> std::same_as<std::vector<Page>>;
};
```

This is the proposition: "T can provide posts and pages." Each line is a proof obligation — the type must provide `all_posts()` returning exactly `std::vector<Post>`, and `all_pages()` returning exactly `std::vector<Page>`. The `->` return-type constraint is a nested requirement: the expression must be well-formed *and* its result must satisfy a further concept.

### WatchPolicy and Reloadable: Independent Capabilities

```cpp
template<typename W>
concept WatchPolicy = requires(W w) {
    { w.poll() } -> std::same_as<std::optional<ChangeSet>>;
    { w.start() } -> std::same_as<void>;
    { w.stop()  } -> std::same_as<void>;
};

template<typename S>
concept Reloadable = requires(S s, const ChangeSet& cs) {
    { s.reload(cs) } -> std::same_as<void>;
};
```

`WatchPolicy` describes change detection. `Reloadable` describes the ability to update from a change set. These are orthogonal concepts — neither subsumes the other — and they combine by conjunction in the hot reloader.

### HotReloader: Conjunction as a Requires Clause

The `HotReloader` class template brings these concepts together:

```cpp
template<typename Source, WatchPolicy Watcher, typename Cache>
    requires ContentSource<Source> && Reloadable<Source>
class HotReloader { /* ... */ };
```

Read this through natural deduction. The `requires` clause is a conjunction:

```
  ContentSource<Source>    Reloadable<Source>
  ──────────────────────────────────────────   (∧-intro)
    ContentSource<Source> ∧ Reloadable<Source>
```

The compiler verifies both proof obligations independently. `Source` must provide posts and pages (`ContentSource`) *and* accept change-set reloads (`Reloadable`). `Watcher` must satisfy `WatchPolicy`. All three propositions must hold simultaneously for the class to be instantiated.

This is modus ponens in action: `HotReloader` says "if these propositions hold, then this class exists." When you instantiate `HotReloader<FileSystemSource, InotifyWatcher, SiteCache>`, the compiler attempts to prove all three propositions. If any proof fails, the class does not exist — instantiation is rejected with a clear error message pointing to the unsatisfied concept.

### Conditional Formation in StrongType

At the method level, Loom uses `requires` for conditional formation:

```cpp
template<typename T, typename Tag>
class StrongType {
    T value_;
public:
    bool empty() const requires requires(const T& v) { v.empty(); }
    { return value_.empty(); }
};
```

The nested `requires` is a proof attempt: "does `v.empty()` compile for the underlying type?" If yes, `StrongType<T, Tag>::empty()` exists. If not, it does not. For `StrongType<std::string, SlugTag>`, the method is available because `std::string` has `empty()`. For a hypothetical `StrongType<int, CountTag>`, the formation rule is not satisfied, and the method simply does not exist — no error unless you try to call it.

This is conditional formation driven by intuitionistic proof search. The compiler does not reason "either `empty()` exists or it doesn't" in the abstract — it *constructs* the proof by checking well-formedness. Success creates the method. Failure removes it from the type's interface.

## Closing: Logic You Already Use

You have been doing logic every time you write a concept, constrain a template, or select between overloads. The vocabulary of natural deduction — introduction, elimination, conjunction, implication, modus ponens — is already embedded in the language. Naming it makes it deliberate.

The practical consequence: treat concept design like proof design. A concept is minimal (only the needed obligations), composable (combines with other concepts via logical connectives), and subsumable (fits into a refinement hierarchy from weak to strong). The compiler is your proof checker. Let it work.

In the [next post](/post/compile-time-data), we explore what happens when *values* enter the type system — dependent types, Pi types, Sigma types, and the compiler as a staged computation engine.
