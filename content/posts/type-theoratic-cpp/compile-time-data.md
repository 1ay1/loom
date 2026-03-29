---
title: "Compile-Time Data — When Values Become Types"
date: 2026-03-25
slug: compile-time-data
tags: [c++20, type-theory, constexpr, nttp, compile-time, dependent-types]
excerpt: "C++20 lets values enter the type system. This is the gateway to dependent types — Pi types, Sigma types, and the calculus of constructions. The compiler becomes a staged computation engine."
---

Throughout this series, we have treated types and values as belonging to different worlds. Types live at compile time. Values live at runtime. The compiler reasons about types; the CPU processes values.

C++20 blurs that line. Non-type template parameters (NTTPs) let you put *values* into the type system. A string can be a template parameter. A struct can be a template parameter. When this happens, the compiler does not just check types — it *computes with data* before the program runs.

This is the closest C++ gets to **dependent types** — and understanding what dependent types actually are reveals both the power and the limits of C++'s approach.

## Dependent Types: The Real Theory

In Martin-Löf type theory (1972), there are two fundamental type constructors beyond functions and pairs:

### Pi Types (Π): Dependent Functions

A **Pi type** `Π(x:A). B(x)` is a function where the *return type depends on the input value*. Not just on the input's type — on the actual value.

In mathematics: "for all x in A, there exists a B(x)." The function takes an x and produces something whose type depends on x.

In C++, the closest approximation:

```cpp
template<int N>
auto make_array() -> std::array<int, N> {
    return {};
}
```

The return type `std::array<int, N>` depends on the *value* N. `make_array<5>()` returns `std::array<int, 5>`. `make_array<10>()` returns `std::array<int, 10>`. Different values, different return types. This is a Pi type — restricted to compile-time constants.

In a fully dependently typed language (Agda, Idris), you could write:

```
make_array : (n : Nat) → Array Int n
```

Where `n` is a *runtime* value, and the return type still depends on it. C++ cannot do this — the template parameter must be `constexpr`. But within that restriction, Pi types are exactly what NTTPs give you.

### Sigma Types (Σ): Dependent Pairs

A **Sigma type** `Σ(x:A). B(x)` is a pair where the *second component's type depends on the first component's value*.

The existential quantifier: "there exists an x in A such that B(x) holds." The pair carries both the witness x and the evidence B(x).

C++ has no direct Sigma type. But consider:

```cpp
struct ParsedRoute {
    bool is_static;
    // If is_static == true, this is a static route with an exact path
    // If is_static == false, this is a dynamic route with parameter positions
    // The "type" of the rest depends on the value of is_static
};
```

In a dependent type system, you would write:

```
ParsedRoute = Σ(b : Bool). if b then StaticData else DynamicData
```

In C++, we approximate this with variants or with compile-time branching via `if constexpr`.

## Non-Type Template Parameters

Since C++98, templates accepted integers and enums as parameters. C++20 opens this to *any structural type*:

```cpp
template<int N>
struct FixedArray {
    int data[N];
};

FixedArray<10> a;  // type encodes the size
FixedArray<20> b;  // different type — different size
```

`FixedArray<10>` and `FixedArray<20>` are different types. The value `10` is part of the type. This IS a dependent type — a type that depends on a value. The restriction: the value must be known at compile time.

## Compile-Time Strings

The key enabler for advanced patterns is strings as template parameters:

```cpp
template<size_t N>
struct Lit {
    char data[N];

    constexpr Lit(const char (&str)[N]) {
        for (size_t i = 0; i < N; ++i)
            data[i] = str[i];
    }

    constexpr auto sv() const -> std::string_view {
        return {data, N - 1};
    }

    constexpr auto size() const -> size_t { return N - 1; }
};
```

Now:

```cpp
template<Lit path>
void handle();

handle<"/api/users">();   // path is part of the type
handle<"/api/orders">();  // different type — different path
```

These are different *instantiations*, each specialized for a specific string. The compiler knows the string at compile time and can generate optimal code.

## Compile-Time Pattern Analysis

Once strings are in the type system, you can analyze them at compile time:

```cpp
template<Lit Pattern>
struct Traits {
    static constexpr bool is_static =
        Pattern.sv().find(':') == std::string_view::npos;

    static constexpr size_t prefix_len = [] {
        auto sv = Pattern.sv();
        auto colon = sv.find(':');
        return colon == std::string_view::npos ? sv.size() : colon;
    }();
};
```

Given `"/post/:slug"`, the compiler evaluates at compile time: `is_static` is `false`, `prefix_len` is 6. These are not runtime computations — they happen during compilation. The binary never contains the analysis code, only its results.

Routing dispatch uses `if constexpr`:

```cpp
template<Lit Pattern>
auto match(std::string_view path) -> bool {
    if constexpr (Traits<Pattern>::is_static) {
        return path == Pattern.sv();
    } else {
        constexpr auto prefix = Pattern.sv().substr(0, Traits<Pattern>::prefix_len);
        return path.starts_with(prefix) && path.size() > prefix.size();
    }
}
```

The `if constexpr` selects the strategy *at compile time*. Static routes get exact comparison. Dynamic routes get prefix matching. The dead branch is not compiled.

## The Lambda Cube Revisited

In [part 1](/post/type-theoretic-foundations) we introduced the lambda cube. NTTPs push C++ further along the "types depending on terms" axis:

| Axis | Lambda Cube | C++ Feature |
|---|---|---|
| Terms → Types | λ2 (System F) | `template<typename T>` |
| Types → Types | λω (Higher-order) | `template<template<typename> typename F>` |
| Terms → Types | λP (Dependent) | `template<int N>`, `template<Lit S>` |

With NTTPs:

```cpp
template<typename T, size_t N>
struct Vec {
    std::array<T, N> data;

    template<size_t M>
    constexpr auto concat(const Vec<T, M>& other) const -> Vec<T, N + M> {
        Vec<T, N + M> result{};
        for (size_t i = 0; i < N; ++i) result.data[i] = data[i];
        for (size_t i = 0; i < M; ++i) result.data[N + i] = other.data[i];
        return result;
    }

    constexpr auto head() const -> T requires (N > 0) {
        return data[0];
    }
};
```

`Vec<int, 5>` and `Vec<int, 3>` are different types. `concat` returns `Vec<int, 8>` — the compiler computes `5 + 3` in the *type*. The `head()` constraint `requires (N > 0)` makes calling it on an empty vector a compile error. This is a dependent type in action.

## The Calculus of Constructions

The full **Calculus of Constructions** (Coquand and Huet, 1988) is the corner of the lambda cube where all three axes are present: terms depend on types, types depend on types, AND types depend on terms. It is the foundation of proof assistants like Coq.

C++ has fragments of all three axes but none completely:

- **Polymorphism**: full (templates can abstract over any type)
- **Type constructors**: full (`template<template<typename> typename>`)
- **Dependent types**: restricted (NTTPs must be `constexpr`)

The restriction to compile-time constants is why C++ is not a dependently typed language. In the Calculus of Constructions, you can write `(n : Nat) → Vec n` where `n` is runtime. In C++, `N` in `Vec<T, N>` must be known at compilation. This keeps type checking decidable — in full dependent types, type checking can be undecidable (equivalent to the halting problem in the worst case).

## Universes and Type-of-Types

In type theory, types themselves have types. The type of `int` is `Type` (or `Set` or `*`). What is the type of `Type`? If `Type : Type`, you get Russell's paradox. The solution is a hierarchy:

```
int : Type₀
Type₀ : Type₁
Type₁ : Type₂
...
```

C++ does not have explicit universes, but template-template parameters hint at the hierarchy:

```cpp
template<typename T>                          // T lives at level 0
void f();

template<template<typename> typename F>       // F lives at level 1 (takes types, returns types)
void g();

template<template<template<typename> typename> typename H>  // H lives at level 2
void h();
```

Each nesting level is a step up the universe hierarchy. C++ does not enforce consistency between levels (no universe polymorphism), but the structure is there.

## Compile-Time Configuration

NTTPs of struct type encode entire configuration objects in the type system:

```cpp
struct ServerConfig {
    int port;
    int max_connections;
    bool enable_tls;
    size_t buffer_size;
};

template<ServerConfig Config>
class Server {
    static constexpr auto BUFFER_SIZE = Config.buffer_size;
    std::array<char, BUFFER_SIZE> read_buffer_;

public:
    void run() {
        if constexpr (Config.enable_tls) {
            init_tls();
        }
        listen(Config.port);
    }
};

using ProdServer = Server<ServerConfig{443, 10000, true, 8192}>;
using DevServer = Server<ServerConfig{8080, 10, false, 1024}>;
```

`ProdServer` and `DevServer` are different types. TLS code is only compiled into `ProdServer`. Buffer sizes are compile-time constants enabling stack allocation.

## Staging and Multi-Stage Programming

`constexpr`/`consteval` is a form of **multi-stage programming**: code that generates code. Stage 0 (compile time) computes values. Stage 1 (runtime) uses them.

```cpp
consteval auto checked_port(int port) -> int {
    if (port < 1 || port > 65535)
        throw "port out of range";  // compile error if triggered
    return port;
}

constexpr auto PORT = checked_port(8080);   // validated at compile time
```

The `throw` in `consteval` causes a compilation failure, not a runtime exception. This is *staged validation*: the check runs in an earlier stage, and only valid results survive to the next stage.

This connects to the theory of **MetaML** and **MetaOCaml** — languages designed around staged computation. C++ is arguably the most widely used staged programming language in the world. The stages are:

1. **Template metaprogramming** — types computing with types
2. **constexpr/consteval evaluation** — values computed at compile time
3. **Template instantiation** — generic code specialized for specific types/values
4. **Optimization** — dead code elimination, inlining, constant propagation
5. **Runtime execution** — the actual program runs

Steps 1-4 are all "compile time" from the programmer's perspective, but they are distinct computation stages. Type-theoretic C++ front-loads work into stages 1 and 2.

## Compile-Time Parsing

When you combine `constexpr` functions with NTTPs, you can parse strings at compile time:

```cpp
struct Route {
    std::string_view method;
    std::string_view pattern;
    bool has_param;
};

constexpr auto parse_route(std::string_view spec) -> Route {
    auto space = spec.find(' ');
    auto method = spec.substr(0, space);
    auto pattern = spec.substr(space + 1);
    bool has_param = pattern.find(':') != std::string_view::npos;
    return {method, pattern, has_param};
}

template<Lit Spec>
struct CompiledRoute {
    static constexpr auto info = parse_route(Spec.sv());
    static constexpr auto method = info.method;
    static constexpr auto pattern = info.pattern;
    static constexpr bool is_parameterized = info.has_param;
};

using PostRoute = CompiledRoute<"GET /post/:slug">;
static_assert(PostRoute::method == "GET");
static_assert(PostRoute::pattern == "/post/:slug");
static_assert(PostRoute::is_parameterized == true);
```

The parsing is ordinary C++ — but it runs during compilation. The binary contains only the pre-parsed results. This is Loom's [router](/post/router) in action.

## The Phase Distinction: A Formal View

In type theory, the **phase distinction** separates compile time from runtime. In a dependently typed language, this distinction blurs because types can depend on runtime values. C++ maintains a *strict* phase distinction: NTTPs must be `constexpr`.

This is a deliberate tradeoff:

- **Strict phase distinction** (C++): type checking is decidable, compilation always terminates, runtime values cannot influence types
- **No phase distinction** (Agda, Idris): types can depend on runtime values, but type checking may not terminate, and you need a totality checker

C++ chose decidability. The price is that `Vec<int, n>` where `n` is a runtime variable is impossible — you use `std::vector` instead. But within the compile-time boundary, you get genuine dependent typing.

## The Limits

C++ compile-time data works for:

- Configuration known at build time
- Protocol definitions, route tables, schema descriptions
- Literal constants (ports, regex patterns, format strings)
- Code generation based on fixed structure

But not for:

- User input, network data, file contents
- Anything determined at runtime

The art is knowing where the boundary falls and designing so that as much structure as possible lives on the compile-time side. Everything decided at compile time is a decision that never costs a cycle at runtime, never fails in production, never depends on external state.

## Closing: The Compiler as an Interpreter

The traditional view: the compiler translates source to machine code. The type-theoretic view: the compiler *executes a program* (the type-level program) and emits the result.

When you write `constexpr auto x = parse_route("GET /post/:slug")`, you are asking the compiler to run a program and embed the result in the binary. Compilation IS execution — in an earlier stage. This is dependent types in practice: values computed at one stage become type-level data at the next.

In the [next post](/post/parametricity), we explore one of the most beautiful results in type theory — parametricity and free theorems: what you can prove about a function from nothing but its type signature.
