---
title: "Compile-Time Data — When Values Become Types"
date: 2026-05-03
slug: compile-time-data
tags: [c++20, type-theory, constexpr, nttp, compile-time, dependent-types]
excerpt: "C++20 lets you pass values as template parameters — strings, structs, even user-defined literals. When data enters the type system, the boundary between types and values dissolves, and the compiler becomes a computation engine."
---

Throughout this series, we have treated types and values as belonging to different worlds. Types live at compile time. Values live at runtime. The compiler reasons about types; the CPU processes values.

C++20 blurs that line. Non-type template parameters (NTTPs) let you put *values* into the type system. A string can be a template parameter. A struct can be a template parameter. A compile-time-computed configuration object can be a template parameter. When this happens, the compiler does not just check types — it *computes with data* before the program runs.

This is the closest C++ gets to **dependent types** — types that depend on values. And it unlocks patterns that are impossible in any other mainstream systems language.

This post builds on [post #9 on constexpr and consteval](/post/constexpr-consteval-and-compile-time) and connects directly to the [compile-time router](/post/router) in Loom, which uses every technique described here.

## Non-Type Template Parameters

Since C++98, templates have accepted non-type parameters — but only of a few types: integers, enums, pointers. C++20 opens this to *any structural type*: a class type where all members are public and themselves structural.

```cpp
template<int N>
struct FixedArray {
    int data[N];
};

FixedArray<10> a;  // type encodes the size
FixedArray<20> b;  // different type — different size
```

`FixedArray<10>` and `FixedArray<20>` are different types. The value `10` is part of the type. The compiler knows the array size at compile time and can optimize accordingly — bounds checks can be eliminated, loops can be unrolled, the stack allocation is exact.

This is ordinary and familiar. What C++20 adds is the ability to do this with *arbitrary data*.

## Compile-Time Strings

The key enabler for many patterns is the ability to use strings as template parameters. C++ does not allow `const char*` NTTPs directly (pointer identity is not a compile-time concept), but you can wrap a string in a structural type:

```cpp
template<size_t N>
struct Lit {
    char data[N];

    constexpr Lit(const char (&str)[N]) {
        for (size_t i = 0; i < N; ++i)
            data[i] = str[i];
    }

    constexpr auto sv() const -> std::string_view {
        return {data, N - 1};  // N includes null terminator
    }

    constexpr auto size() const -> size_t { return N - 1; }
};
```

`Lit` copies the string literal into a fixed-size array. Because the array size is part of the template parameter `N`, and the array contents are `constexpr`, the entire string lives in the type system.

Now you can write:

```cpp
template<Lit path>
void handle();

handle<"/api/users">();   // path is part of the type
handle<"/api/orders">();  // different type — different path
```

These are two different *instantiations* of `handle`, each specialized for a specific string. The compiler knows the string at compile time and can generate optimal code for each one — pattern matching via `if constexpr`, compile-time length checks, even compile-time parsing of the string contents.

## Compile-Time Pattern Analysis

Once strings are in the type system, you can analyze them at compile time. Loom's [router](/post/router) does exactly this:

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

Given the pattern `"/post/:slug"`, the compiler evaluates at compile time: `is_static` is `false` (the pattern contains `:`), and `prefix_len` is 6 (the length of `"/post/"`). These are not runtime computations. They happen during compilation. The binary never contains the analysis code — only its results.

This means routing dispatch can use `if constexpr`:

```cpp
template<Lit Pattern>
auto match(std::string_view path) -> bool {
    if constexpr (Traits<Pattern>::is_static) {
        return path == Pattern.sv();  // exact comparison
    } else {
        constexpr auto prefix = Pattern.sv().substr(0, Traits<Pattern>::prefix_len);
        return path.starts_with(prefix) && path.size() > prefix.size();
    }
}
```

The `if constexpr` selects the matching strategy *at compile time*. Static routes get exact string comparison. Dynamic routes get prefix matching. The dead branch is not compiled. At `-O2`, each route's matcher becomes a single comparison instruction or a short prefix check. No hash map, no trie, no runtime pattern analysis.

## Dependent Types (Almost)

In type theory, a **dependent type** is a type that depends on a value. The canonical example is a vector whose type includes its length: `Vec<int, 5>` is a different type than `Vec<int, 3>`, and a function that concatenates them returns `Vec<int, 8>`.

C++ gets surprisingly close:

```cpp
template<typename T, size_t N>
struct Vec {
    std::array<T, N> data;

    template<size_t M>
    constexpr auto concat(const Vec<T, M>& other) const -> Vec<T, N + M> {
        Vec<T, N + M> result;
        for (size_t i = 0; i < N; ++i) result.data[i] = data[i];
        for (size_t i = 0; i < M; ++i) result.data[N + i] = other.data[i];
        return result;
    }

    constexpr auto head() const -> T requires (N > 0) {
        return data[0];
    }
};
```

`Vec<int, 5>` and `Vec<int, 3>` are different types. `concat` returns `Vec<int, 8>` — the compiler computes `5 + 3` and encodes the result in the return type. The `head()` method has a `requires (N > 0)` constraint — calling it on an empty vector is a *compile error*, not a runtime error.

This is not full dependent types (the sizes must be compile-time constants, not runtime values), but it is a remarkable approximation. The compiler tracks the length through operations and verifies constraints at every step.

## Compile-Time Configuration

NTTPs of struct type let you encode entire configuration objects in the type system:

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
        // Config.port, Config.enable_tls, etc. are all constexpr
        if constexpr (Config.enable_tls) {
            init_tls();
        }
        listen(Config.port);
    }
};

// Two completely different server types, optimized for their config
using ProdServer = Server<ServerConfig{443, 10000, true, 8192}>;
using DevServer = Server<ServerConfig{8080, 10, false, 1024}>;
```

`ProdServer` and `DevServer` are different types. The TLS initialization code is only compiled into `ProdServer` — the `if constexpr` eliminates it entirely from `DevServer`. The buffer sizes are known at compile time, enabling stack allocation with exact sizes. The configuration is not read from a file at startup — it is embedded in the type and optimized by the compiler.

## Literal Operators and User-Defined Literals

C++ lets you define custom literal suffixes, enabling a syntax where values enter the type system through natural notation:

```cpp
template<Lit S>
constexpr auto operator""_path() {
    // Compile-time validation: path must start with /
    static_assert(S.sv().starts_with('/'), "paths must start with /");
    return S;
}

// These are compile-time validated:
constexpr auto api = "/api/users"_path;    // OK
// constexpr auto bad = "api/users"_path;  // ERROR: paths must start with /
```

The `static_assert` runs at compile time. An invalid path is not a runtime error — it is a compilation failure. The error message is clear and immediate.

## Compile-Time Parsing

When you combine `constexpr` functions with NTTPs, you can *parse* strings at compile time and encode the parse result in the type system:

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

// The string "GET /post/:slug" is parsed at compile time:
using PostRoute = CompiledRoute<"GET /post/:slug">;
static_assert(PostRoute::method == "GET");
static_assert(PostRoute::pattern == "/post/:slug");
static_assert(PostRoute::is_parameterized == true);
```

The parsing function `parse_route` is ordinary C++ — loops, string operations, conditionals. But because it is `constexpr` and its input is a compile-time string, the entire computation happens during compilation. The binary contains only the pre-parsed results. The parsing code is gone.

This is what makes Loom's router possible. The route patterns are parsed, analyzed, and compiled into an optimal dispatch chain — all before the program runs.

## consteval: Forcing Compile Time

`constexpr` functions *can* run at compile time but are also valid at runtime. `consteval` functions *must* run at compile time — calling them with runtime arguments is an error:

```cpp
consteval auto checked_port(int port) -> int {
    if (port < 1 || port > 65535)
        throw "port out of range";  // compile error if triggered
    return port;
}

constexpr auto PORT = checked_port(8080);   // OK — evaluated at compile time
// constexpr auto BAD = checked_port(99999); // ERROR: "port out of range"
```

The `throw` in a `consteval` function does not throw at runtime — it causes a compilation failure with the error message. This turns `consteval` into a compile-time assertion mechanism with custom error messages.

This is the compile-time equivalent of the "parse, don't validate" pattern from [part 1](/post/type-theoretic-foundations). Instead of validating at runtime, you validate at compile time. The `checked_port` function is a proof that the port is in range. If it returns, the value is valid. If it does not, the program does not compile.

## The Computation Phase

When you combine everything — `constexpr` functions, compile-time strings, NTTPs of struct type, `consteval`, `if constexpr` — the compiler becomes a computation engine that runs a program before the program runs.

The phases of a C++20 program:

1. **Compile-time computation** — `constexpr`/`consteval` functions evaluate, NTTPs are instantiated, `if constexpr` selects branches, `static_assert` checks invariants
2. **Template instantiation** — generic code is specialized for specific types and values
3. **Optimization** — the compiler eliminates dead code, inlines functions, propagates constants
4. **Code generation** — machine code is emitted for what remains
5. **Runtime execution** — the actual program runs

Steps 1-4 are all "compile time" from the programmer's perspective, but they are distinct phases. Type-theoretic C++ front-loads as much work as possible into steps 1 and 2, so that by step 5, the program is a lean, pre-computed artifact with no unnecessary generality.

## The Limit of the Approach

C++ is not a dependently-typed language. The values that enter the type system must be compile-time constants. You cannot write `Vec<int, n>` where `n` is a runtime variable (that would be `std::vector`). You cannot parse a user-provided string at compile time (the string must be a literal).

This is a real limitation. It means compile-time data techniques work for:

- Configuration that is known at build time
- Protocol definitions, route tables, schema descriptions
- Literal constants (port numbers, regex patterns, format strings)
- Code generation based on fixed structure

But not for:

- User input
- Network data
- File contents (unless embedded at build time)
- Anything determined at runtime

The art is knowing where the boundary falls and designing your system so that as much structure as possible lives on the compile-time side.

## Closing: The Compiler as an Interpreter

The traditional view: the compiler translates source code into machine code. The type-theoretic view: the compiler *runs a program* (the type-level program) and then emits the result.

When you write `constexpr auto x = parse_route("GET /post/:slug")`, you are not asking the compiler to "optimize" a runtime computation. You are asking it to execute a program and embed the result in the binary. The compilation process *is* execution — just in a different phase.

This changes how you think about program structure. Instead of "write code that runs fast," you think: "what can I compute *before* the program starts?" Every decision made at compile time is a decision that never costs a cycle at runtime, never fails in production, never depends on external state.

In the [final post](/post/type-safe-protocol) of this series, we put every technique together — phantom types, typestate, concepts, compile-time data — to build a real, type-safe protocol stack where the compiler verifies the entire interaction sequence before a single byte crosses the wire.
