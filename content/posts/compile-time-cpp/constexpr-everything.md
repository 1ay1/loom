---
title: "constexpr Everything — Real Algorithms Running Inside Your Compiler"
date: 2026-02-23
slug: constexpr-everything
tags: compile-time-cpp, constexpr, consteval, constinit, immediate-functions
excerpt: constexpr started as a way to mark simple constants. Then it grew. And grew. Now it's an entire execution engine inside the compiler, running real algorithms with loops, branches, allocations, and exceptions.
---

For most of this series, we've been programming in the compile-time language through templates — the type-level functional language with angle brackets and `::type`. That language is powerful, but it's hostile. Writing a loop requires recursion. Writing a conditional requires specialization. Every operation is a new template instantiation. The error messages are archaeology.

`constexpr` is the second path. Instead of programming in the template metalanguage, you write normal C++ — loops, branches, variables, mutation — and the compiler evaluates it during compilation. Same result: computation happens before runtime. Completely different experience: the code looks like code.

This post traces the evolution of `constexpr` from its crippled C++11 origins to the near-complete execution environment it is today. Along the way, we'll see what you can do, what you still can't, and why `consteval` exists to force the issue.

## C++11: The Timid Beginning

C++11 introduced `constexpr`, and immediately disappointed everyone who wanted to do anything interesting with it. The rules were severe:

```cpp
constexpr int factorial(int n) {
    return n <= 1 ? 1 : n * factorial(n - 1);
}
```

That's about the limit. A `constexpr` function in C++11 had to be a single `return` statement. No variables. No loops. No `if` statements (you could use the ternary operator, barely). No `void` return type. No multiple statements of any kind.

You could compute factorials and Fibonacci numbers. You could do simple arithmetic. You could write the kind of functions that make good interview questions and terrible production code.

The restriction existed because compiler implementers weren't sure how far they wanted to go. Evaluating C++ at compile time means building an interpreter inside the compiler. C++11 dipped a toe in. The water was fine.

## C++14: Actually Useful

C++14 relaxed the rules dramatically. `constexpr` functions could now contain:

- Local variables (including mutable ones)
- Multiple statements
- `if`/`else` (real `if`, not ternary)
- `for` and `while` loops
- Multiple `return` statements

Suddenly, you could write real code:

```cpp
constexpr int factorial(int n) {
    int result = 1;
    for (int i = 2; i <= n; ++i) {
        result *= i;
    }
    return result;
}

static_assert(factorial(10) == 3628800);
```

That's a normal imperative function. Loops, mutation, a local variable — everything the C++11 version forbade. The compiler evaluates it step by step, exactly as a CPU would, except no CPU is involved. The compiler *is* the CPU.

This was the inflection point. Before C++14, `constexpr` was a curiosity. After C++14, it was a programming model. People started writing constexpr sort algorithms, constexpr hash functions, constexpr parsers. The old template-metaprogramming patterns — recursive templates, type lists, SFINAE — suddenly had competition from code that looked like... code.

## C++17: constexpr if and Lambda

C++17 added two features that made constexpr significantly more expressive.

**`if constexpr`** lets you write branches where the untaken path doesn't need to compile (covered in [Control Flow at Compile Time](/post/control-flow-at-compile-time)). Inside a constexpr function, this means you can write type-dependent logic without SFINAE:

```cpp
template<typename T>
constexpr auto serialize(const T& value) {
    if constexpr (std::is_integral_v<T>) {
        return static_cast<int64_t>(value);
    } else if constexpr (std::is_floating_point_v<T>) {
        return static_cast<double>(value);
    } else {
        return value.to_string();
    }
}
```

**constexpr lambdas.** Lambdas became implicitly `constexpr` when possible in C++17. This meant you could use lambdas inside constexpr computations, and pass constexpr lambdas to algorithms:

```cpp
constexpr auto transform_and_sum() {
    std::array<int, 5> vals = {1, 2, 3, 4, 5};
    auto square = [](int x) { return x * x; };

    int total = 0;
    for (auto v : vals) {
        total += square(v);
    }
    return total;
}

static_assert(transform_and_sum() == 55); // 1 + 4 + 9 + 16 + 25
```

Lambda inside a constexpr function, captured nothing, called at compile time. Perfectly normal. The compiler evaluates the lambda call just like any other function call during constexpr evaluation.

## C++20: The Big Leap

C++20 is where `constexpr` stopped being "some functions can run at compile time" and became "almost everything can run at compile time." The additions were sweeping:

### constexpr std::string and std::vector

Covered briefly in [Data Structures at Compile Time](/post/data-structures-at-compile-time), but worth revisiting here because it's genuinely remarkable. `std::string` and `std::vector` do dynamic memory allocation. The compiler now has a memory allocator.

```cpp
constexpr auto count_words(std::string_view text) {
    std::vector<std::string_view> words;
    std::size_t pos = 0;

    while (pos < text.size()) {
        while (pos < text.size() && text[pos] == ' ') ++pos;
        auto start = pos;
        while (pos < text.size() && text[pos] != ' ') ++pos;
        if (start < pos) {
            words.push_back(text.substr(start, pos - start));
        }
    }

    return words.size();
}

static_assert(count_words("the quick brown fox") == 4);
static_assert(count_words("") == 0);
static_assert(count_words("   spaces   everywhere   ") == 2);
```

The `std::vector` is created, elements are pushed back, `size()` is called, and the vector is destroyed — all during compilation. The constraint is **transient allocation**: every `new` must have a matching `delete` before the constexpr evaluation completes. Memory allocated at compile time cannot leak into runtime. If it does, the program is ill-formed.

This matters because it means `constexpr` functions can use all the standard library algorithms that work with dynamic containers. `std::sort`, `std::unique`, `std::partition` — they all work at compile time now, because the containers they operate on work at compile time.

```cpp
constexpr auto sorted_unique_chars(std::string_view input) {
    std::vector<char> chars(input.begin(), input.end());
    std::sort(chars.begin(), chars.end());
    chars.erase(std::unique(chars.begin(), chars.end()), chars.end());

    // Can't return the vector (non-transient allocation), so copy to array
    std::array<char, 26> result{};
    std::size_t count = std::min(chars.size(), result.size());
    for (std::size_t i = 0; i < count; ++i) {
        result[i] = chars[i];
    }
    return std::pair{result, count};
}

constexpr auto [chars, n] = sorted_unique_chars("hello world");
static_assert(n == 8); // ' ', 'd', 'e', 'h', 'l', 'o', 'r', 'w'
```

Sort and unique at compile time. The result is baked into the binary as a constant array. The `std::vector` existed only inside the compiler's head.

### constexpr virtual Functions

C++20 allows virtual function calls in constexpr contexts. The compiler resolves the virtual dispatch at compile time (since it knows the dynamic type):

```cpp
struct Shape {
    constexpr virtual double area() const = 0;
    constexpr virtual ~Shape() = default;
};

struct Circle : Shape {
    double radius;
    constexpr Circle(double r) : radius(r) {}
    constexpr double area() const override { return 3.14159265358979 * radius * radius; }
};

struct Square : Shape {
    double side;
    constexpr Square(double s) : side(s) {}
    constexpr double area() const override { return side * side; }
};

constexpr double total_area() {
    Circle c{5.0};
    Square s{3.0};
    const Shape* shapes[] = {&c, &s};

    double total = 0;
    for (auto* shape : shapes) {
        total += shape->area();
    }
    return total;
}

static_assert(total_area() > 87.0);
```

Polymorphism at compile time. The compiler sees through the virtual dispatch because it has complete knowledge of the types involved. There's no vtable lookup — the compiler knows `shapes[0]` is a `Circle` and `shapes[1]` is a `Square` because it's tracking the entire evaluation.

### constexpr try/catch

C++20 technically allows `try`/`catch` blocks in constexpr functions. However, throwing an exception during constexpr evaluation is not allowed — it's a compile error if the throw is actually reached. The `try`/`catch` syntax is permitted so that the same function can work at both compile time and runtime:

```cpp
constexpr int safe_divide(int a, int b) {
    if (b == 0) {
        throw std::domain_error("division by zero"); // fine if never reached
    }
    return a / b;
}

constexpr int x = safe_divide(10, 2);   // OK: 5
// constexpr int y = safe_divide(10, 0); // compile error: throw reached
int z = safe_divide(10, 0);             // OK at compile time, throws at runtime
```

This is a clever dual-use pattern. At compile time, dividing by zero is a compile error — caught before the program exists. At runtime, it throws an exception — caught by whatever error handling you have. Same function, different error semantics depending on when it's evaluated. The compile-time version is strictly better: you find the bug earlier.

### constexpr Algorithms

C++20 marked most of `<algorithm>` as `constexpr`. `std::sort`, `std::find`, `std::count`, `std::transform`, `std::accumulate` (in `<numeric>`) — they all work at compile time now.

```cpp
constexpr auto build_frequency_table() {
    std::string_view text = "abracadabra";
    std::array<int, 26> freq{};
    for (char c : text) {
        freq[c - 'a']++;
    }
    return freq;
}

constexpr auto freq = build_frequency_table();
static_assert(freq['a' - 'a'] == 5);  // 'a' appears 5 times
static_assert(freq['b' - 'a'] == 2);  // 'b' appears 2 times
static_assert(freq['z' - 'a'] == 0);  // 'z' doesn't appear
```

This is a frequency counter built at compile time. The input is a string literal. The output is an array embedded in the binary. The computation between them — iterating, counting, indexing — happens entirely inside the compiler.

## consteval: No Escape to Runtime

`constexpr` functions are dual-use. They *can* run at compile time, but they can also run at runtime if called with runtime arguments. `consteval` (C++20) removes the runtime option entirely. A `consteval` function *must* be evaluated at compile time. If you try to call it with a runtime value, it's a compile error.

```cpp
consteval int compile_time_only(int x) {
    return x * x;
}

constexpr int a = compile_time_only(5);  // OK: compile-time evaluation
int b = compile_time_only(5);             // OK: argument is a constant

int n = 5;
// int c = compile_time_only(n);          // Error: n is not a constant expression
```

Why would you want this? Three reasons.

**Safety.** If a function validates data that must be validated at compile time (format strings, SQL queries, configuration schemas), you don't want it silently falling back to runtime evaluation where it adds overhead and might miss inputs.

**Performance guarantees.** A `consteval` function is guaranteed to produce a compile-time constant. There's no ambiguity about whether the optimizer will evaluate it or not. The result is always a constant.

**Better error messages.** When a `consteval` function can't be evaluated at compile time, the error comes from the `consteval` declaration, not from some deep template instantiation. "This function must be evaluated at compile time, and it can't be because argument X isn't a constant."

The term for `consteval` functions is **immediate functions**. They're evaluated immediately at the call site, during compilation, with no possibility of deferral. Every call to an immediate function must produce a constant. If it doesn't, the program is ill-formed.

### consteval as a Checkpoint

A powerful pattern: use `consteval` as a validation checkpoint in an otherwise runtime computation.

```cpp
consteval void validate_port(int port) {
    if (port < 0 || port > 65535) {
        throw "port number out of range"; // compile error if reached
    }
}

struct ServerConfig {
    int port;
    std::string host;

    consteval ServerConfig(int p, const char* h) : port(p), host(h) {
        validate_port(p);
    }
};

constexpr ServerConfig config{8080, "localhost"};      // OK
// constexpr ServerConfig bad{99999, "localhost"};       // compile error: port out of range
```

The validation runs at compile time. If the port is invalid, you get a compile error. Not a runtime crash. Not a log message. Not an exception that might get swallowed. A hard compile error that prevents the program from being built with bad configuration.

This pattern applies to anything with invariants you know at compile time: enum ranges, array sizes, protocol constants, magic numbers. `consteval` constructors turn your types into compile-time-validated types. If the constructor succeeds, the invariants hold. If it doesn't, the code doesn't compile.

## constinit: The Initialization Order Problem

C++ has a nasty problem called the "static initialization order fiasco." If you have global variables in different translation units, and their initialization depends on each other, the order is undefined. Your program might work in debug builds, crash in release builds, and work again with a different compiler.

`constinit` (C++20) solves one piece of this. It ensures a variable is initialized at compile time — constant initialization, before any dynamic initialization runs. The variable itself doesn't need to be `const`:

```cpp
constinit int global_count = 42;       // initialized at compile time
constinit thread_local int tls_val = 0; // compile-time initialized, thread-local

void reset() {
    global_count = 0;  // OK: it's not const, you can modify it at runtime
}
```

The difference from `constexpr`: a `constexpr` variable is `const` (immutable at runtime). A `constinit` variable has a compile-time-known initial value but can be modified at runtime.

`constinit` doesn't give you any new compile-time computation abilities. Its purpose is purely about initialization ordering — guaranteeing that a variable is ready before `main()` starts, before any dynamic initialization happens, before any static initialization order fiasco can manifest. It's the compile-time language solving a runtime problem.

## What You Still Can't Do

The constexpr evaluator has limits. Some are fundamental, some are being actively eroded.

**No I/O.** You can't read files, print to the console, or make network calls at compile time. The compiler is a hermetically sealed environment. The only input is the source code; the only output is the binary (and diagnostics). This is by design — compilation should be deterministic and reproducible.

**No reinterpret_cast.** You can't reinterpret memory as a different type. `reinterpret_cast` is explicitly banned in constexpr contexts because it bypasses the type system, and the constexpr evaluator relies on the type system to track what's happening.

**No inline assembly.** Obviously.

**No thread creation.** The constexpr evaluator is single-threaded. You can't spawn threads, use mutexes, or do any concurrent programming at compile time. (There's nothing to be concurrent *with* — the compiler is already using all available parallelism for compilation.)

**Transient allocation only** (as discussed). Memory allocated at compile time must be freed at compile time. You can't create a compile-time `std::vector` and use it at runtime. You can compute with one, but you need to copy the results into a non-allocating type before the constexpr evaluation ends.

**Step limits.** Compilers impose limits on how many steps a constexpr evaluation can take. GCC defaults to 33 million operations (`-fconstexpr-ops-limit`). Clang defaults to about 1 million steps (`-fconstexpr-steps`). These limits prevent infinite loops from hanging the compiler, but they also constrain legitimate long computations. You can raise them with compiler flags, but if your constexpr computation hits the default limit, it might be time to question whether the compiler is the right place to run it.

**No `goto`.** Even though `goto` exists in C++, it's not allowed in constexpr functions. The constexpr evaluator uses structured control flow. This is rarely a practical limitation — structured code is better code — but it's there.

## The Compiler as Interpreter

Here's the thing about constexpr that's easy to miss: the compiler contains a complete C++ interpreter. Not a subset. Not a simplified version. A real interpreter that can execute loops, call functions, allocate memory, dispatch virtual calls, evaluate lambdas, run standard library algorithms, and throw exceptions.

This interpreter is AST-walking — it executes the program by traversing its abstract syntax tree, rather than generating machine code and running it. This is slower than native execution (often much slower), which is why step limits exist. But it's functionally complete for the subset of C++ that constexpr supports.

When you write a constexpr function, you're writing a program that runs twice: once inside the compiler (as interpreted C++), and potentially again at runtime (as compiled machine code). The compile-time execution can validate inputs, precompute tables, verify invariants, and produce constants that the runtime code uses directly.

This is genuinely unusual. Most languages have a clear separation between the compilation phase and the execution phase. C++ blurs that line. The compilation phase *is* an execution phase — for constexpr code. And with `consteval`, you can ensure that certain computations *only* happen during that phase.

## Practical Patterns

### Compile-Time Lookup Table

```cpp
constexpr auto build_crc32_table() {
    std::array<uint32_t, 256> table{};
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t crc = i;
        for (int j = 0; j < 8; ++j) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
        table[i] = crc;
    }
    return table;
}

constexpr auto crc32_table = build_crc32_table();
```

A CRC32 lookup table, computed at compile time. 256 entries, each requiring 8 iterations of bit manipulation. The compiler does 2048 iterations of the inner loop, produces 256 uint32_t values, and embeds them in the binary as a constant array. At runtime, `crc32_table` is just data — no initialization code, no function call, no startup overhead.

This is the same table that CRC32 implementations have been shipping as hardcoded constants for decades. The difference is that this one is *computed*, not *transcribed*. If the polynomial changes, you change one constant. The table recomputes itself.

### Compile-Time Hash Function

```cpp
constexpr uint64_t fnv1a_hash(std::string_view s) {
    uint64_t hash = 14695981039346656037ull;
    for (char c : s) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 1099511628211ull;
    }
    return hash;
}

constexpr uint64_t operator""_hash(const char* s, std::size_t len) {
    return fnv1a_hash({s, len});
}

void handle_command(std::string_view cmd) {
    switch (fnv1a_hash(cmd)) {
        case "start"_hash:  start();  break;
        case "stop"_hash:   stop();   break;
        case "reload"_hash: reload(); break;
        default: unknown_command(cmd); break;
    }
}
```

The string hashes in the `case` labels are computed at compile time via the `constexpr` user-defined literal operator. The `switch` compiles to a jump table or binary search over integer constants — no string comparisons in the dispatch path. The hash of the input `cmd` is computed at runtime (it has to be — it's runtime data), but the target hashes are constants.

This pattern is used in game engines, command dispatchers, and protocol parsers where you need to switch on strings without the overhead of `std::unordered_map`.

### Compile-Time State Machine Validation

```cpp
enum class State { Idle, Running, Paused, Stopped };
enum class Event { Start, Pause, Resume, Stop };

struct Transition {
    State from;
    Event event;
    State to;
};

constexpr Transition transitions[] = {
    {State::Idle,    Event::Start,  State::Running},
    {State::Running, Event::Pause,  State::Paused},
    {State::Running, Event::Stop,   State::Stopped},
    {State::Paused,  Event::Resume, State::Running},
    {State::Paused,  Event::Stop,   State::Stopped},
};

consteval bool validate_state_machine() {
    // Check: every state (except Stopped) has at least one outgoing transition
    for (auto state : {State::Idle, State::Running, State::Paused}) {
        bool has_transition = false;
        for (auto& t : transitions) {
            if (t.from == state) { has_transition = true; break; }
        }
        if (!has_transition) return false;
    }

    // Check: no duplicate transitions (same from + event)
    for (int i = 0; i < std::size(transitions); ++i) {
        for (int j = i + 1; j < std::size(transitions); ++j) {
            if (transitions[i].from == transitions[j].from &&
                transitions[i].event == transitions[j].event) {
                return false;
            }
        }
    }

    return true;
}

static_assert(validate_state_machine(), "State machine has unreachable states or duplicate transitions");
```

The state machine is defined as data. The validation is a `consteval` function that checks the invariants. If someone adds a conflicting transition or leaves a state disconnected, the code doesn't compile. The invariant check runs during compilation and costs nothing at runtime.

### Compile-Time Regular Expression (Simplified)

```cpp
constexpr bool matches_pattern(std::string_view text, std::string_view pattern) {
    std::size_t ti = 0, pi = 0;

    while (pi < pattern.size()) {
        if (pattern[pi] == '*') {
            // '*' matches zero or more characters
            ++pi;
            if (pi == pattern.size()) return true;  // trailing * matches everything
            while (ti < text.size()) {
                if (matches_pattern(text.substr(ti), pattern.substr(pi)))
                    return true;
                ++ti;
            }
            return false;
        }
        if (ti >= text.size()) return false;
        if (pattern[pi] != '?' && pattern[pi] != text[ti]) return false;
        ++ti;
        ++pi;
    }

    return ti == text.size();
}

static_assert(matches_pattern("hello.cpp", "*.cpp"));
static_assert(matches_pattern("test_main.cc", "test_*.*"));
static_assert(!matches_pattern("readme.md", "*.cpp"));
static_assert(matches_pattern("a", "?"));
static_assert(!matches_pattern("ab", "?"));
```

A glob-style pattern matcher, running at compile time. Recursive backtracking for `*`, single-character match for `?`, literal match for everything else. The `static_assert` proves the patterns are correct — or disproves them, with a compile error.

This isn't a production regex engine, but the technique scales. CTRE (Compile-Time Regular Expressions) uses the same principle — parse a regex string at compile time, produce a specialized matcher — to achieve performance competitive with hand-written parsing code.

## The Two Paths Converge

Here's where the series comes full circle.

Post 1 established that C++ has two languages: a runtime language and a compile-time language. Posts 2 through 8 taught the template metalanguage — the original compile-time language, with types as values and templates as functions. This post taught `constexpr` — the second compile-time language, where normal C++ code runs inside the compiler's interpreter.

These two paths solve the same problem differently:

**Template metaprogramming** operates on types. It's functional, recursive, and type-level. It's the right tool when your inputs and outputs are types: type lists, type transformations, compile-time dispatch.

**constexpr** operates on values. It's imperative, iterative, and value-level. It's the right tool when your inputs and outputs are data: lookup tables, hash functions, parsers, validators.

In practice, you use both. A `fixed_string` NTTP passes a string into the template system (type-level). A constexpr function parses it (value-level). The parse result drives template instantiation (type-level again). The instantiated template generates runtime code. The boundaries between the two compile-time languages blur constantly, and the best C++ metaprogramming uses whichever is clearer for each step.

The compile-time language isn't one language. It's two. Templates for types. `constexpr` for values. Both execute during compilation. Both dissolve before the binary exists. And together, they give you programming capabilities that no runtime-only approach can match.
