---
title: "Templates — Writing Code That Works for Any Type"
date: 2025-09-12
slug: templates-and-generic-programming
tags: cpp, templates, generic-programming, nttp
excerpt: Templates aren't generics. They're a code generation system disguised as a type system feature.
---

Templates are the defining feature of C++. Not classes, not RAII, not even move semantics — templates. They're what make the standard library work. They're what make zero-cost abstractions possible. They're what make Loom's compile-time routing, HTML DSL, and CSS DSL exist without runtime overhead.

And they're simpler than you think. At their core, templates are just parameterized code. You write the code once, parameterized by types or values, and the compiler generates concrete versions for each combination you actually use.

## Function Templates

A function template is a recipe for generating functions:

```cpp
template<typename T>
T max_of(T a, T b) {
    return (a > b) ? a : b;
}

int x = max_of(3, 7);           // generates max_of<int>
double y = max_of(1.5, 2.3);    // generates max_of<double>
```

The compiler sees `max_of(3, 7)`, deduces that `T = int`, and generates a concrete `max_of<int>(int, int)` function. This is called template *instantiation*. The generated function is exactly as efficient as if you'd written it by hand.

The standard library is built on this. `std::sort`, `std::find_if`, `std::vector` — all templates. When you call `std::sort(vec.begin(), vec.end(), comparator)`, the compiler generates a sort function specialized for your exact vector type and comparator. No virtual calls, no indirection, no overhead.

## Class Templates

A class template parameterizes an entire type:

```cpp
template<typename T, typename Tag>
class StrongType {
public:
    explicit StrongType(T value) : value_(std::move(value)) {}
    T get() const { return value_; }

    bool operator==(const StrongType& other) const { return value_ == other.value_; }
    bool operator<(const StrongType& other) const { return value_ < other.value_; }

private:
    T value_;
};
```

This is Loom's `StrongType` — a wrapper that prevents type confusion. You parameterize it with the underlying type and a tag:

```cpp
struct SlugTag {};
struct TitleTag {};
struct TagTag {};

using Slug = StrongType<std::string, SlugTag>;
using Title = StrongType<std::string, TitleTag>;
using Tag = StrongType<std::string, TagTag>;
```

`Slug`, `Title`, and `Tag` are all wrappers around `std::string`, but they're different types. You can't pass a `Slug` where a `Title` is expected:

```cpp
void set_title(const Title& t);

Slug s("hello");
// set_title(s);  // COMPILE ERROR: Slug is not Title
set_title(Title("hello"));  // OK
```

The tag types (`SlugTag`, `TitleTag`) carry no data — they exist purely to make the template generate distinct types. This is a phantom type pattern: a type parameter that affects the type identity but not the runtime behavior or memory layout.

At runtime, `StrongType<std::string, SlugTag>` has the exact same layout and performance as `std::string`. The compiler generates a wrapper class, but after optimization, the wrapper is completely erased — it's just a string with compiler-enforced boundaries.

## Class Template Argument Deduction (CTAD)

C++17 introduced CTAD, which lets the compiler deduce template arguments from constructor arguments:

```cpp
std::vector v = {1, 2, 3};                    // deduced as vector<int>
std::pair p = {"hello", 42};                   // deduced as pair<const char*, int>
std::optional opt = std::string("hello");      // deduced as optional<string>
```

Loom's route DSL uses a deduction guide for the `Lit` struct:

```cpp
template<size_t N>
Lit(const char (&)[N]) -> Lit<N>;
```

This tells the compiler: when someone writes `Lit("hello")`, deduce `N = 6` (5 characters plus the null terminator). The user never writes `Lit<6>` — the size is deduced from the string literal.

## Non-Type Template Parameters (NTTPs)

This is where templates diverge from Java/C# generics. Template parameters don't have to be types — they can be values:

```cpp
template<size_t N>
struct Lit {
    char buf[N]{};

    constexpr Lit(const char (&s)[N]) noexcept {
        for (size_t i = 0; i < N; ++i) buf[i] = s[i];
    }

    constexpr std::string_view sv() const noexcept { return {buf, N - 1}; }
    constexpr size_t size() const noexcept { return N - 1; }
};
```

The `N` in `Lit<N>` is not a type — it's a `size_t` value baked into the type at compile time. `Lit<6>` and `Lit<10>` are different types. The compiler knows the string length at compile time and can optimize accordingly.

C++20 extended NTTPs to support *class types* — not just integers. This is what enables Loom's route patterns:

```cpp
template<Lit P>
struct Traits {
    static consteval bool is_static() noexcept {
        for (size_t i = 0; i < P.size(); ++i)
            if (P[i] == ':') return false;
        return true;
    }
};
```

`P` is a `Lit` value — a compile-time string. The `Traits` struct analyzes the pattern at compile time, determining whether it's a static route (like `"/"`) or a parameterized route (like `"/post/:slug"`). The analysis runs entirely at compile time. At runtime, it's just a boolean constant.

## Template Instantiation: Why It's Zero-Cost

This is the key insight that distinguishes templates from runtime polymorphism. When you write:

```cpp
auto dispatch = compile(
    fallback(not_found_handler),
    get<"/">(index_handler),
    get<"/post/:slug">(post_handler)
);
```

The compiler generates a concrete `Compiled` struct with three specific routes. There's no route table in memory, no hash map of strings, no trie. The patterns `"/"` and `"/post/:slug"` are baked into the code as compile-time constants. The dispatch function is a sequence of `if` checks comparing string_views against constants.

This is what "zero-cost abstraction" means: the abstraction (route DSL, pattern matching, parameter extraction) exists in your source code but disappears completely in the generated machine code. The CPU sees the same instructions it would see if you'd written the if-else chain by hand.

Compare this to a runtime router that stores routes in a `vector<Route>` and iterates over them, or a trie that traverses nodes. Those approaches require heap allocation, indirection, and cache misses. The template approach requires none of that.

## The Route Template

Here's Loom's `Route` type, which ties everything together:

```cpp
template<HttpMethod M, Lit P, typename H>
struct Route {
    H handler;

    constexpr explicit Route(H h) : handler(std::move(h)) {}

    bool try_dispatch(HttpRequest& req, HttpResponse& out) const {
        if (req.method != M) return false;
        if (!Traits<P>::match(req.path)) return false;

        if constexpr (!Traits<P>::is_static())
            req.params.emplace_back(Traits<P>::param(req.path));

        out = handler(req);
        return true;
    }
};
```

Three template parameters:
- `M`: an `HttpMethod` enum value (GET, POST, etc.)
- `P`: a `Lit` value (the URL pattern)
- `H`: the handler type (a lambda, function pointer, or any callable)

Each combination creates a unique type. `Route<GET, "/", IndexHandler>` and `Route<GET, "/about", AboutHandler>` are different types. The compiler can inline the handler call, eliminate dead branches (via `if constexpr`), and generate optimal matching code for each route individually.

## Why Zero-Cost

Here's the mental model: templates are a code generation system. When the compiler sees `get<"/post/:slug">(handler)`, it doesn't build a data structure representing a route. It generates a specific `Route` struct with:

- Method check: `req.method != HttpMethod::GET` — a single integer comparison
- Pattern match: `path.starts_with("/post/")` — a single memcmp
- Parameter extraction: `path.substr(6)` — a pointer adjustment

No virtual dispatch. No string parsing at runtime. No heap allocation. The template generates exactly the code you'd write by hand, but the abstraction gives you a clean DSL to work with.

This is the promise of C++ templates: write high-level, expressive code that compiles to low-level, optimal machine code. The gap between abstraction and performance doesn't exist — the compiler closes it entirely.

The DOM DSL works the same way. When you write `div(class_("container"), h1("Hello"))`, the compiler generates code that appends elements to vectors — there's no parser, no interpreter, no dynamic dispatch. The HTML structure is expressed in C++ and compiled to direct function calls.

Understanding templates is understanding why C++ abstractions are different from abstractions in other languages. In Python, every abstraction has a runtime cost. In Java, generics are erased at runtime. In C++, templates generate specialized code — the abstraction is real at the source level and gone at the machine level.

Next: variadic templates and fold expressions, where templates go from "parameterize one type" to "parameterize any number of types."
