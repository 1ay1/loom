---
title: C++20 Concepts in Loom — Constraints Without SFINAE
date: 2024-03-10
slug: cpp20-concepts
tags: cpp, internals
excerpt: Loom uses C++20 Concepts to constrain template parameters for content sources and watch policies. Here's what concepts are, how they work, and why they're better than SFINAE.
---

C++20 Concepts let you state requirements on template parameters directly, in readable syntax, with clear error messages. Loom uses them to define what it means to be a "content source" and a "watch policy." This post covers the concepts Loom uses, how they work under the hood, and why they're a meaningful improvement over C++17 SFINAE.

## The Problem Concepts Solve

Suppose you have a template function:

```cpp
template<typename Source>
void load(Source& s) {
    for (const auto& post : s.all_posts()) { ... }
}
```

If you call `load(42)`, you get an error message like:

```
error: expression '42.all_posts()' is not valid
  in instantiation of function template 'load<int>'
    called from ...
    [20 more lines of template stack]
```

With a concept, you get:

```
error: 'int' does not satisfy 'ContentSource'
  constraint 'requires { s.all_posts() }' not satisfied
```

The concept names what's wrong. The template stack is gone. This matters for everyone reading compiler errors — which is everyone.

## Defining a Concept

```cpp
template<typename T>
concept ContentSource = requires(T source) {
    { source.all_posts() }  -> std::same_as<std::vector<Post>>;
    { source.all_pages() }  -> std::same_as<std::vector<Page>>;
    { source.site_config() } -> std::convertible_to<SiteConfig>;
};
```

A concept is a boolean predicate on a type. `ContentSource<T>` is true if `T` has those three methods with those return types.

The `requires(T source) { ... }` block is a *requires expression*. Inside it:
- `{ expr }` checks that the expression compiles
- `{ expr } -> SomeType` also checks that the expression's return type satisfies `SomeType`
- `std::same_as<V>` is a concept requiring exact type match
- `std::convertible_to<V>` is a concept requiring implicit convertibility

## Concepts in Loom

### ContentSource

```cpp
template<typename T>
concept ContentSource = requires(T source) {
    { source.all_posts()  } -> std::same_as<std::vector<Post>>;
    { source.all_pages()  } -> std::same_as<std::vector<Page>>;
    { source.site_config()} -> std::convertible_to<SiteConfig>;
};
```

Both `FileSystemSource` and `GitSource` satisfy this. The hot reloader is templated on a ContentSource — it doesn't care whether content comes from disk or git, as long as those methods exist.

### WatchPolicy

```cpp
template<typename W>
concept WatchPolicy = requires(W w) {
    { w.poll()  } -> std::same_as<std::optional<ChangeSet>>;
    { w.start() } -> std::same_as<void>;
    { w.stop()  } -> std::same_as<void>;
};
```

`InotifyWatcher` (filesystem) and `GitWatcher` both satisfy this. `poll()` returns `nullopt` when nothing changed, or a `ChangeSet` describing what did.

### Reloadable

```cpp
template<typename T>
concept Reloadable = requires(T source, const ChangeSet& cs) {
    { source.reload(cs) } -> std::same_as<void>;
};
```

Content sources that support incremental reload (only reloading posts that changed, not all content) satisfy Reloadable. The hot reloader calls `reload(changeset)` instead of rebuilding from scratch.

## Using Concepts in Templates

```cpp
template<ContentSource Source, WatchPolicy Watcher>
class HotReloader {
    Source&  source_;
    Watcher  watcher_;
    // ...
public:
    void start() {
        thread_ = std::thread([this] {
            watcher_.start();
            while (running_) {
                auto change = watcher_.poll();
                if (change) rebuild(*change);
                std::this_thread::sleep_for(100ms);
            }
            watcher_.stop();
        });
    }
};
```

`template<ContentSource Source, WatchPolicy Watcher>` is the shorthand syntax. It's equivalent to the verbose form:

```cpp
template<typename Source, typename Watcher>
    requires ContentSource<Source> && WatchPolicy<Watcher>
```

The shorthand is cleaner. The verbose form is useful when you need to name the type for use in the body (though you can also use the concept name directly with `auto`).

## The `requires` Clause on Member Functions

The `StrongType` class uses a `requires` clause on a member function:

```cpp
bool empty() const
    requires requires(const T& v) { v.empty(); }
{ return value_.empty(); }
```

The outer `requires` gates the existence of the member function. The inner `requires(const T& v) { v.empty(); }` is a requires expression — it's `true` if `v.empty()` compiles.

So `StrongType<std::string, Tag>::empty()` exists, but `StrongType<int, Tag>::empty()` doesn't — calling it would be a compile error.

This is called *conditional member functions* — the member is part of the type only when the constraint is satisfied.

## Abbreviated Function Templates

C++20 also introduces abbreviated function templates with `auto`:

```cpp
// Traditional template
template<typename T>
void print(const T& value) { std::cout << value; }

// Abbreviated
void print(const auto& value) { std::cout << value; }
```

Both declare a function template over any type. The abbreviated form is useful in lambdas too:

```cpp
auto sorted_by_date = [](const auto& a, const auto& b) {
    return a.published < b.published;
};
std::sort(posts.begin(), posts.end(), sorted_by_date);
```

Loom uses this pattern throughout — lambdas with `const auto&` are cleaner than `template<typename T>` for short predicates.

## Concepts vs SFINAE

SFINAE (Substitution Failure Is Not An Error) is the C++11/14/17 technique for constraining templates. The same `ContentSource` concept in SFINAE:

```cpp
template<typename T, typename = void>
struct is_content_source : std::false_type {};

template<typename T>
struct is_content_source<T, std::void_t<
    decltype(std::declval<T>().all_posts()),
    decltype(std::declval<T>().all_pages()),
    decltype(std::declval<T>().site_config())
>> : std::true_type {};

template<typename T>
constexpr bool is_content_source_v = is_content_source<T>::value;

template<typename Source,
    std::enable_if_t<is_content_source_v<Source>, int> = 0>
class HotReloader { ... };
```

That's 12 lines for what concepts do in 5. And the error message when the constraint fails is the full SFINAE substitution chain rather than a named concept.

Concepts aren't just syntax sugar — they change how errors are reported, when constraints are checked (at the call site, not deep in template instantiation), and how overload resolution works (concepts can be used to select between template specialisations in ways SFINAE can't express cleanly).

## Checking a Concept at Compile Time

You can check whether a type satisfies a concept with `static_assert`:

```cpp
static_assert(ContentSource<FileSystemSource>,
    "FileSystemSource must satisfy ContentSource");
static_assert(WatchPolicy<InotifyWatcher>,
    "InotifyWatcher must satisfy WatchPolicy");
```

Put these near the implementations. If a refactor breaks the interface, you get a clear error at the assert rather than a cryptic failure at the call site.
