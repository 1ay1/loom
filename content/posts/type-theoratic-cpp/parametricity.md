---
title: "Parametricity — Theorems for Free"
date: 2026-03-27
slug: parametricity
tags: [c++20, type-theory, parametricity, free-theorems, polymorphism]
excerpt: "If a function works for any type and never inspects it, its behavior is constrained by the type signature alone. You get theorems without proofs. This is parametricity — and it explains why phantom types, generic algorithms, and templates are more powerful than they look."
---

In 1989, Philip Wadler published a paper called "Theorems for Free!" with an exclamation mark that was entirely earned. The paper showed that from the *type signature alone* of a polymorphic function — without reading a single line of the implementation — you can deduce non-trivial facts about its behavior.

This result, called **parametricity**, is one of the most beautiful ideas in type theory. It explains why generic programming works, why phantom types ([part 4](/post/phantom-types)) are safe, and why the most constrained code is often the most powerful.

## The Core Idea

If a function is polymorphic — it works for any type T — and it does not inspect T at runtime, then it must treat all T uniformly. It cannot branch on what T is. It cannot conjure T values from thin air. It cannot peek inside T. It can only shuffle T values around according to the structure visible in the type signature.

This constraint is not a limitation. It is a *guarantee*. It means you can reason about the function's behavior without reading its body.

## The Identity Function

Start with the simplest example:

```cpp
template<typename T>
auto id(T x) -> T;
```

What can this function do? It receives a value of type T and must return a value of type T. Consider its options:

- It cannot create a T from nothing — it does not know what T is (maybe T has no default constructor, maybe it is a private class)
- It cannot modify the T — it does not know T's interface (maybe T has no methods)
- It cannot return a *different* T — where would it get one?

The **only** thing it can do is return its argument unchanged. The type signature `∀T. T → T` has exactly one inhabitant: the identity function.

This is remarkable. You proved a theorem about the function — it must be the identity — without reading the implementation. The proof comes entirely from the type.

## More Free Theorems

### Functions on pairs

```cpp
template<typename A, typename B>
auto fst(std::pair<A, B> p) -> A;
```

What can this function do? It must return an A. It has one A available (inside the pair). It has no way to create an A or transform the B into an A (it knows nothing about A or B). It must return `p.first`.

Similarly, `template<typename A, typename B> auto snd(std::pair<A, B>) -> B` must return `p.second`.

### Functions choosing between arguments

```cpp
template<typename T>
auto choose(T x, T y) -> T;
```

This function takes two T values and returns one T. It cannot create a new T. It cannot combine them (it does not know T's operations). It must return either `x` or `y`. Furthermore, its *choice* cannot depend on the *values* of x and y (it cannot inspect T). So it must consistently return the first argument for ALL types, or consistently return the second for ALL types. There are exactly two inhabitants of this type: "always first" and "always second."

### Functions on vectors

```cpp
template<typename T>
auto rearrange(std::vector<T> v) -> std::vector<T>;
```

This function takes a vector of T and returns a vector of T. It cannot create new T values. It cannot modify existing ones. It can only rearrange — reorder, duplicate, or drop elements. The output vector's elements must be a subset (with possible repetition) of the input.

Free theorem: for any function `g : A → B`:

```
map(g, rearrange(v)) == rearrange(map(g, v))
```

The rearrangement commutes with mapping. Whatever structural transformation `rearrange` performs, it does the same thing regardless of the element type. You can apply `g` before or after — the result is the same.

This is the **naturality condition** from category theory. Parametric polymorphism gives you naturality for free.

### The filter-map commutation

```cpp
template<typename T>
auto my_filter(std::vector<T> v, std::function<bool(const T&)> pred)
    -> std::vector<T>;
```

The `pred` function introduces a wrinkle — it can inspect T values. But `my_filter` itself is parametric in T. Free theorem: for any `g : A → B`:

```
map(g, my_filter(v, pred)) == my_filter(map(g, v), pred ∘ g⁻¹)
```

...provided the predicate is compatible with the mapping. When the predicate does not depend on the specific representation, filter and map commute.

## Reynolds' Relational Interpretation

The theoretical foundation is John Reynolds' **relational parametricity** (1983). The idea:

For any polymorphic function `f : ∀T. F(T)`, and any *relation* R between types A and B:

> If you apply f to R-related inputs, you get R-related outputs.

A "relation" between types A and B is a correspondence — which A values are "related to" which B values. A function `g : A → B` induces a functional relation: a is related to b if `g(a) = b`.

**Example**: Take `id : ∀T. T → T` and the relation induced by `g : int → string` where `g(n) = std::to_string(n)`.

Parametricity says: if we apply `id` to both sides, related inputs give related outputs.

```
id(n) is related to id(g(n))
⟹  n is related to g(n)       // since id(x) = x
⟹  g(id(n)) = id(g(n))       // which is trivially true
```

For `rearrange`:

```
g(rearrange(v)) = rearrange(g(v))   // where g is applied elementwise
```

This is the free theorem. It follows mechanically from the type signature and the relational interpretation.

## Parametricity in C++

C++ templates are *not* automatically parametric. C++ provides several escape hatches that break parametricity:

### Type inspection at compile time

```cpp
template<typename T>
auto describe(T x) -> std::string {
    if constexpr (std::is_integral_v<T>) {
        return "integer: " + std::to_string(x);
    } else if constexpr (std::is_floating_point_v<T>) {
        return "float: " + std::to_string(x);
    } else {
        return "unknown";
    }
}
```

This function is NOT parametric — it branches on the type. Different types get different behavior. No free theorems apply.

### Template specialization

```cpp
template<typename T>
struct Formatter {
    static auto format(T x) -> std::string { return "generic"; }
};

template<>
struct Formatter<int> {
    static auto format(int x) -> std::string { return std::to_string(x); }
};
```

Specialization provides different implementations for different types. Parametricity is broken by design.

### Runtime type inspection

```cpp
template<typename T>
void inspect(T& x) {
    if (typeid(x) == typeid(int)) { /* ... */ }
}
```

`typeid` and `dynamic_cast` allow runtime type inspection, violating parametricity entirely.

### When parametricity holds

Parametricity holds in C++ when you **avoid all of the above**: no `if constexpr` on type traits, no template specialization, no `typeid`, no SFINAE tricks that branch on type identity. In other words, when you write a template that genuinely treats T as an opaque box:

```cpp
template<typename T>
auto swap_pair(std::pair<T, T> p) -> std::pair<T, T> {
    return {p.second, p.first};
}
```

This function is parametric. Free theorem: for any `g : A → B`:

```
map_pair(g, swap_pair(p)) == swap_pair(map_pair(g, p))
```

Swapping commutes with mapping. The function does not know what T is, so it must treat all T identically.

## Ad-Hoc vs Parametric Polymorphism

Christopher Strachey drew this distinction in 1967:

**Parametric polymorphism**: the same code for all types. A template without specialization, without type-trait branching. The behavior is uniform. Free theorems apply.

**Ad-hoc polymorphism**: different code for different types. Function overloading, template specialization, concept-based dispatch. The behavior varies. No free theorems.

C++ has both, intertwined. A `template<typename T>` with no constraints is parametric. Add `requires Printable<T>` and it becomes ad-hoc — different behavior for types that satisfy `Printable` vs those that do not (the function does not exist for the latter).

Knowing which kind of polymorphism you are using matters for reasoning:

```cpp
// Parametric — free theorems apply
template<typename T>
auto reverse(std::vector<T> v) -> std::vector<T>;

// Ad-hoc — no free theorems
template<typename T>
    requires std::totally_ordered<T>
auto sort(std::vector<T> v) -> std::vector<T>;
```

`reverse` must commute with map. `sort` need not — sorting integers may produce a different permutation than sorting strings, because the ordering relation differs.

## Why Phantom Types Work: The Parametricity Explanation

In [part 4](/post/phantom-types), we built `Strong<Tag, T>` — a wrapper parameterized by a phantom tag. The tag is never stored or accessed. Why is this safe?

Because code parametric in `Tag` cannot inspect the tag. Consider:

```cpp
template<typename Tag>
auto increment(Strong<Tag, int> s) -> Strong<Tag, int> {
    return Strong<Tag, int>{s.value + 1};
}
```

Free theorem: for any two tags `A` and `B`, if `Strong<A, int>` and `Strong<B, int>` contain the same `int` value, then `increment` produces outputs containing the same `int` value. The function cannot distinguish tags. It cannot convert a `Strong<PassengerTag>` into a `Strong<FlightTag>`. Parametricity *proves* tag safety.

This is the formal justification behind the informal claim in part 4. Phantom types are safe because parametricity constrains what tag-polymorphic code can do.

## Practical Benefits of Parametric Code

**Easier to test.** If a parametric function works correctly for `int`, it works for all types. You need fewer test cases because uniformity is guaranteed by the type.

**Easier to optimize.** The compiler can assume uniformity — no hidden branches on type identity. Inlining and specialization are straightforward.

**Self-documenting.** The type signature constrains behavior so tightly that the implementation is (sometimes) determined. `∀T. T → T` must be identity. `∀A B. (A,B) → A` must be `fst`. The types are the documentation.

**More compositional.** Parametric functions compose predictably. If `f` and `g` both commute with `map`, then `f ∘ g` commutes with `map`. Naturality is preserved under composition.

## The Abstraction Theorem

Reynolds' **abstraction theorem** (the formal statement of parametricity) says:

> Every term in System F (the polymorphic lambda calculus) satisfies the logical relation induced by its type.

In English: if the type system says a function is polymorphic, the function *must* behave polymorphically. There is no way to fake it. The type system does not just *describe* uniformity — it *enforces* it.

C++ is not System F. It has escape hatches (specialization, type traits, RTTI). But within the fragment of C++ that is genuinely parametric — templates without specialization, without `if constexpr` on type identity — the abstraction theorem holds. And that fragment is large enough to be useful.

## In Loom

Loom uses parametric polymorphism in several key components. Each demonstrates free theorems in production code.

### AtomicCache<T>: Parametric in T

The `AtomicCache` in `include/loom/reload/hot_reloader.hpp` is a lock-free, atomically-swappable cache:

```cpp
template<typename T>
class AtomicCache
{
public:
    explicit AtomicCache(std::shared_ptr<const T> initial)
        : data_(std::move(initial)) {}

    std::shared_ptr<const T> load() const noexcept
    {
        return data_.load(std::memory_order_acquire);
    }

    void store(std::shared_ptr<const T> next) noexcept
    {
        data_.store(std::move(next), std::memory_order_release);
    }

private:
    std::atomic<std::shared_ptr<const T>> data_;
};
```

`AtomicCache` is parametric in `T`. It never inspects the cached value — it cannot, because it knows nothing about `T`. It only moves `shared_ptr<const T>` in and out. The free theorem: for any function `g : A → B`, if you transform the cached value via `g`, the cache's load/store behavior is identical. The cache is a container that shuffles pointers — the content is opaque.

This is precisely why the cache is correct for any content type. Whether it holds a `SiteCache`, a `Config`, or anything else, the atomic swap semantics are identical. Parametricity guarantees that `AtomicCache` cannot corrupt or depend on the structure of the data it holds.

### HotReloader: Parametric in the Cache Type

The `HotReloader` is generic in `Source`, `Watcher`, and `Cache`:

```cpp
template<typename Source, WatchPolicy Watcher, typename Cache>
    requires ContentSource<Source> && Reloadable<Source>
class HotReloader
{
public:
    using RebuildFn = std::function<std::shared_ptr<const Cache>(Source&, const ChangeSet&)>;
    // ...
};
```

The `RebuildFn` is parametric in `Cache`. The reloader calls `rebuild_(source_, pending)` and passes the result to `cache_.store()`. It never inspects the `Cache` value — it treats it as an opaque blob. Free theorem: for any transformation `g` on cache values, applying `g` before or after the reload yields the same observable behavior. The reloader is a scheduling mechanism, not a data processor.

Note that `Source` and `Watcher` are constrained by concepts (`ContentSource`, `WatchPolicy`). These constraints make the polymorphism *ad-hoc* for those parameters — the reloader calls `source_.reload()` and `watcher_.poll()`, which are interface-specific operations. But the `Cache` parameter has no constraints beyond being a type. It is genuinely parametric, and the free theorem applies.

### StrongType<T, Tag>: Parametric in Tag

Loom's phantom type wrapper in `include/loom/core/strong_type.hpp`:

```cpp
template<typename T, typename Tag>
class StrongType
{
public:
    explicit StrongType(T value) : value_(std::move(value)) {}
    T get() const { return value_; }
    bool operator==(const StrongType& other) const { return value_ == other.value_; }
    // ...
private:
    T value_;
};
```

`StrongType` is parametric in `Tag`. The tag is never stored, never inspected, never used at runtime. The free theorem: for any two tags `A` and `B`, if `StrongType<std::string, A>` and `StrongType<std::string, B>` hold the same string, then every tag-polymorphic operation produces the same underlying result. Tag-polymorphic code cannot distinguish a `Slug` from a `Title` — it must treat them uniformly.

This is exactly the parametricity guarantee from the phantom types discussion. The formal justification is not just an analogy: `StrongType` never branches on `Tag`, never specializes for `Tag`, never uses `if constexpr` to inspect it. The code is genuinely parametric, and the abstraction theorem applies. A `Slug` cannot be silently converted to a `Title` because parametricity constrains what tag-generic code can do.

## Closing: Constraints as Theorems

The usual view of types is restrictive: types prevent you from doing things. Parametricity inverts this. Types do not just prevent errors — they *prove properties*. The more polymorphic a function's type, the less it can do, and therefore the more you know about what it *does* do.

A function `∀T. T → T` is maximally constrained and maximally known. A function `int → int` is minimally constrained and minimally known (it could be any function on integers).

This is the paradox of polymorphism: **the less a function can do, the more you know about it.** Generality is not vagueness — it is precision. And that precision comes for free, directly from the type.

In the [next post](/post/substructural-types), we explore what happens when we restrict not just *what* you can do with values, but *how many times* — substructural types, linear logic, and the formal theory behind RAII and move semantics.
