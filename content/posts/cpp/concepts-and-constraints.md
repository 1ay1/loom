---
title: "C++20 Concepts — Naming What Types Must Be"
date: 2025-10-03
slug: concepts-and-constraints
tags: cpp, concepts, constraints, cpp20
excerpt: Before concepts, template errors were a wall of text. Now they're a sentence.
---

Templates are powerful, but they've always had an ugly problem: when you pass the wrong type, the error message is incomprehensible. A hundred lines of nested template substitution failures, pointing at code deep inside the standard library, when all you wanted to know was "you forgot to define the `poll()` method."

C++20 concepts fix this. A concept is a named set of requirements on a type. Instead of discovering requirements through substitution failure, you state them upfront. The compiler checks them at the point of use and gives you a clear error when they're not met.

## Defining a Concept

A concept is a compile-time predicate on types:

```cpp
template<typename T>
concept ContentSource =
requires(T source)
{
    { source.all_posts() } -> std::same_as<std::vector<Post>>;
    { source.all_pages() } -> std::same_as<std::vector<Page>>;
};
```

This is Loom's `ContentSource` concept. It says: a type `T` is a ContentSource if, given an instance `source`, you can call `source.all_posts()` and get a `std::vector<Post>`, and call `source.all_pages()` and get a `std::vector<Page>`.

That's it. No inheritance. No virtual functions. No base class. Just requirements. Any type that satisfies these requirements is a ContentSource, regardless of what it inherits from or how it's implemented.

Loom has two content sources: `FilesystemSource` (reads markdown files from disk) and `GitSource` (reads from a git repository). Both satisfy `ContentSource` by having the right methods with the right signatures. They share no base class.

## The WatchPolicy Concept

Here's a more complex concept:

```cpp
template<typename W>
concept WatchPolicy = requires(W w)
{
    { w.poll() } -> std::same_as<std::optional<ChangeSet>>;
    { w.start() } -> std::same_as<void>;
    { w.stop()  } -> std::same_as<void>;
};
```

A `WatchPolicy` must have three methods: `poll()` returning `optional<ChangeSet>`, `start()` returning void, and `stop()` returning void. The `InotifyWatcher`, `GitWatcher`, and `NullWatcher` all satisfy this concept.

The beauty is in how it's verified:

```cpp
static_assert(WatchPolicy<InotifyWatcher>);
```

This line at the bottom of `inotify_watcher.hpp` proves at compile time that `InotifyWatcher` satisfies the concept. If someone modifies the class and breaks the interface — renames `poll()`, changes its return type, removes `start()` — the static_assert fails with a clear message.

## The Reloadable Concept

```cpp
template<typename S>
concept Reloadable = requires(S s, const ChangeSet& cs)
{
    { s.reload(cs) } -> std::same_as<void>;
};
```

Simple, focused, one requirement: the type must have a `reload` method that takes a `const ChangeSet&` and returns void. This concept captures exactly one behavioral contract.

## Using Concepts as Constraints

Concepts become powerful when used to constrain templates:

```cpp
template<typename Source, WatchPolicy Watcher, typename Cache>
    requires ContentSource<Source> && Reloadable<Source>
class HotReloader { /* ... */ };
```

This is Loom's `HotReloader`. The template parameters have constraints:

- `Watcher` must satisfy `WatchPolicy` (shorthand syntax — the concept name replaces `typename`)
- `Source` must satisfy both `ContentSource` and `Reloadable` (requires clause)
- `Cache` has no constraints — it can be any type

If you try to instantiate `HotReloader` with a watcher that doesn't have a `poll()` method, the compiler says something like:

```
error: template constraint not satisfied
note: 'MyWatcher' does not satisfy 'WatchPolicy'
note: the expression 'w.poll()' is not valid
```

Compare this to pre-concepts error messages, which would be twenty lines of template substitution failure pointing at the internals of `HotReloader` where `w.poll()` is called. The concept error tells you *what's wrong* (the constraint isn't met) and *why* (the expression is invalid).

## Requires on Members

Concepts can require that a type has specific members or operators:

```cpp
bool empty() const requires requires(const T& v) { v.empty(); }
{ return value_.empty(); }
```

This is from Loom's `StrongType`. The `requires requires` (yes, two `requires`) syntax means: "this method exists only if the wrapped type T has an `empty()` method." If T is `std::string`, then `StrongType<std::string, SlugTag>` has `empty()`. If T is `int`, it doesn't.

The first `requires` is a constraint on the method. The second `requires` introduces a requires-expression that checks if `v.empty()` is valid. It looks bizarre at first, but the logic is precise: "require that the expression `v.empty()` is valid."

## The Double requires

Let's break down `requires requires`:

```cpp
// requires-clause: constrains a template or function
template<typename T>
void foo(T x) requires SomeConcept<T>;

// requires-expression: checks if expressions are valid
requires(T x) { x.bar(); }  // evaluates to true/false

// Combined: requires (clause) requires (expression)
template<typename T>
void foo(T x) requires requires(T y) { y.bar(); };
```

The first `requires` says "this template has a constraint." The second `requires` introduces an inline constraint expression. In practice, you define a named concept and use the first form:

```cpp
template<typename T>
concept HasBar = requires(T x) { x.bar(); };

template<HasBar T>
void foo(T x);
```

This is cleaner and reusable. Named concepts are almost always preferable to inline `requires requires`.

## Standard Concepts

The `<concepts>` header provides common concepts:

```cpp
#include <concepts>

std::same_as<T, U>       // T and U are the same type
std::derived_from<D, B>   // D derives from B
std::convertible_to<F, T> // F is convertible to T
std::integral<T>           // T is an integral type
std::floating_point<T>     // T is a floating-point type
std::copyable<T>           // T can be copied
std::movable<T>            // T can be moved
std::invocable<F, Args...> // F can be called with Args
```

Loom's concepts use `std::same_as` in their return type constraints:

```cpp
{ source.all_posts() } -> std::same_as<std::vector<Post>>;
```

This says the return type must be *exactly* `std::vector<Post>` — not a subclass, not a convertible type, not a reference to one. Exact match. This is stricter than inheritance-based polymorphism but also clearer.

## Concepts vs. SFINAE

Before concepts, the way to constrain templates was SFINAE (Substitution Failure Is Not An Error). SFINAE works by making invalid template instantiations silently fail, causing the compiler to try other overloads:

```cpp
// SFINAE: enable this overload only if T has .empty()
template<typename T,
         typename = std::enable_if_t<
             std::is_member_function_pointer_v<decltype(&T::empty)>>>
bool is_empty(const T& x) { return x.empty(); }
```

Compare to the concept version:

```cpp
template<typename T>
concept HasEmpty = requires(const T& v) { v.empty(); };

template<HasEmpty T>
bool is_empty(const T& x) { return x.empty(); }
```

The concept version is readable. The SFINAE version requires expertise in template metaprogramming to parse. They do the same thing, but concepts express the *intent* while SFINAE expresses the *mechanism*.

SFINAE is still needed occasionally for backward compatibility or when working with pre-C++20 code. But for new code, concepts are strictly better — clearer to write, clearer to read, clearer error messages.

## Design Philosophy: Small, Composable Concepts

Loom's concepts are small. `ContentSource` requires two methods. `WatchPolicy` requires three. `Reloadable` requires one. Each concept captures a single behavioral contract.

This is deliberate. Small concepts compose:

```cpp
requires ContentSource<Source> && Reloadable<Source>
```

The `HotReloader` needs a source that can provide content *and* reload itself. Two small concepts combined via `&&`. You can read the constraint as English: "Source must be a ContentSource and Reloadable."

Compare this to inheritance-based design:

```cpp
class ReloadableContentSource : public ContentSource, public Reloadable {
    // Must inherit from two abstract classes
    // Must define all virtual methods
    // Pays for vtable even when not needed
};
```

With concepts, the requirements exist at the type level without affecting the object's runtime representation. No vtable, no virtual dispatch, no base class pointer. The concept is checked at compile time and has zero runtime cost. The compiler generates the same code as if there were no constraint at all — the constraint only affects which types are accepted.

## When to Use Concepts

Use concepts when:
- A template parameter must satisfy a specific interface
- You want clear error messages for invalid types
- You're designing a plugin or extension point (like WatchPolicy)
- You want to document type requirements in a machine-checkable way

Don't use concepts for:
- Simple templates where the requirements are obvious
- Internal implementation details that only you call
- Cases where a concrete type would be simpler

Loom uses concepts at architectural boundaries: where the content system meets the reload system, where the watcher meets the hot reloader. At these boundaries, the concept documents the contract between subsystems. Within a subsystem, concrete types and direct function calls are simpler and sufficient.

The real power of concepts isn't the syntax — it's the way they change your thinking. Instead of "what base class should this inherit from?", you ask "what must this type be able to do?" That's a more fundamental question, and concepts let you answer it directly.

Next: std::variant — when a value can be one of several things, and you need to handle every case.
