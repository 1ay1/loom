---
title: "Compile-Time Strings and Parsing — Reading Text Inside the Compiler"
date: 2026-02-19
slug: compile-time-strings-and-parsing
tags: compile-time-cpp, fixed-string, constexpr-parsing, nttp, string-literals
excerpt: String literals live at compile time. With the right tools, you can parse them, validate them, and turn them into optimized code — all before your program runs.
---

Here's a fun thought experiment: when you write `"hello"` in your C++ code, where does that string live?

It lives at compile time. The characters `h`, `e`, `l`, `l`, `o` are baked into your binary by the compiler. They exist, fully formed, during compilation. The compiler *knows* them. It can see every character.

And yet, for most of C++'s history, the compile-time language — the one with templates and type traits and pattern matching — was completely blind to them. You could pass integers as template arguments, enumerators, even pointers to global objects. But not strings. The most ubiquitous form of structured data in programming, and templates couldn't touch them.

C++20 changed this. And the consequences are wild. Once strings are visible to the template machinery, you can *parse* them at compile time. Format string validation. URL route parsing. Compile-time regex. Embedded DSLs. The entire category of "text in, code out" transformations becomes available without code generation tools or macros.

This post covers how it works, why it matters, and some genuinely impressive things you can do with it.

## Why Strings Were Off-Limits

A string literal `"hello"` has type `const char[6]` (five characters plus the null terminator). It's an array. Before C++20, arrays couldn't be non-type template parameters.

You could *technically* use a pointer:

```cpp
template<const char* S>  // legal, but basically useless
struct message {};

message<"hello"> m;  // nope! string literal can't bind to this
```

The pointer form requires a global variable with external linkage. You can't just pass a string literal. You'd need:

```cpp
constexpr char hello[] = "hello";
message<hello> m;  // OK in C++17, but come on
```

Every distinct string needs its own global variable. The ergonomics are terrible. It's like being told "you can eat dessert, but first you need to fill out this form in triplicate." Technically possible. Practically unusable.

## fixed_string: The Key That Unlocks Everything

C++20 allows class types as non-type template parameters (NTTPs), provided the class is a *structural type* — all members public, no mutable or volatile members. A simple struct wrapping a char array qualifies:

```cpp
template<std::size_t N>
struct fixed_string {
    char data[N];

    constexpr fixed_string(const char (&str)[N]) {
        std::copy_n(str, N, data);
    }

    constexpr std::size_t size() const { return N - 1; }
    constexpr char operator[](std::size_t i) const { return data[i]; }
    constexpr bool operator==(const fixed_string&) const = default;

    constexpr operator std::string_view() const {
        return {data, N - 1};
    }
};

// Deduction guide: lets the compiler figure out N from the literal
template<std::size_t N>
fixed_string(const char (&)[N]) -> fixed_string<N>;
```

The deduction guide is the hero here. It lets the compiler deduce `N` from the literal, so you never specify the size. Now you can write:

```cpp
template<fixed_string S>
struct greeting {
    static void print() {
        std::cout << std::string_view(S) << "\n";
    }
};

greeting<"hello">::print();  // prints "hello"
greeting<"world">::print();  // prints "world"
```

Each distinct string literal produces a distinct template instantiation. `greeting<"hello">` and `greeting<"world">` are *different types*. The compiler can see the full content of the string at instantiation time. Which means you can branch on it, parse it, transform it — all during compilation.

Let me say that again: the compiler can read every character of a string literal and make decisions based on what it finds. That's not just a template trick. That's giving the compile-time language *literacy*.

## Compile-Time String Operations

Since `fixed_string` is constexpr, you can write constexpr functions that operate on it:

```cpp
template<std::size_t N>
constexpr std::size_t find_char(const fixed_string<N>& s, char c, std::size_t pos = 0) {
    for (std::size_t i = pos; i < s.size(); ++i) {
        if (s[i] == c) return i;
    }
    return std::size_t(-1);  // not found
}

template<std::size_t N, std::size_t M>
constexpr bool starts_with(const fixed_string<N>& haystack, const fixed_string<M>& prefix) {
    if (prefix.size() > haystack.size()) return false;
    for (std::size_t i = 0; i < prefix.size(); ++i) {
        if (haystack[i] != prefix[i]) return false;
    }
    return true;
}
```

Find a character. Check a prefix. These are normal string functions. The only unusual thing is that the compiler runs them, not the CPU. The same loops, the same comparisons, the same logic — just evaluated during compilation.

## Compile-Time Parsing: Where It Gets Wild

String operations are nice, but the real payoff is *parsing* — reading a string and producing structured data. This is where compile-time strings go from "convenient" to "holy shit."

### Format String Validation

`std::format` validates its format string at compile time. If you write `std::format("{} is {} years old", name)`, the compiler rejects it — two placeholders but only one argument. Here's how the core idea works:

```cpp
template<fixed_string Fmt>
constexpr std::size_t count_placeholders() {
    std::size_t count = 0;
    for (std::size_t i = 0; i < Fmt.size(); ++i) {
        if (Fmt[i] == '{') {
            if (i + 1 < Fmt.size() && Fmt[i + 1] == '}') {
                ++count;
                ++i;
            }
        }
    }
    return count;
}

template<fixed_string Fmt, typename... Args>
void checked_print(Args&&... args) {
    static_assert(count_placeholders<Fmt>() == sizeof...(Args),
        "format string placeholder count doesn't match argument count");
    // actual formatting logic...
}

checked_print<"{} is {} years old">("Alice", 30);  // OK
checked_print<"{} is {} years old">("Alice");       // COMPILE ERROR
```

The format string is parsed during compilation. The `static_assert` fires if the counts don't match. No runtime cost. No possibility of a format string bug reaching production. The compiler counted the placeholders and checked against the argument count. At compile time. Before the program existed.

### Route Parsing for Web Frameworks

Web frameworks parse URL patterns like `"/users/{id}/posts/{post_id}"` and extract parameter names. Why do this at runtime when the routes are string literals?

```cpp
struct route_param {
    std::size_t start;
    std::size_t length;
};

template<fixed_string Pattern, std::size_t MaxParams = 8>
struct parsed_route {
    route_param params[MaxParams]{};
    std::size_t param_count = 0;

    constexpr parsed_route() {
        for (std::size_t i = 0; i < Pattern.size(); ++i) {
            if (Pattern[i] == '{') {
                std::size_t start = i + 1;
                while (i < Pattern.size() && Pattern[i] != '}') ++i;
                params[param_count++] = {start, i - start};
            }
        }
    }
};

constexpr auto route = parsed_route<"/users/{id}/posts/{post_id}">{};
static_assert(route.param_count == 2);
```

The entire URL pattern is parsed during compilation. At runtime, `route.param_count` is just the constant `2`. The compiler can unroll loops, eliminate branches, and generate a zero-overhead URL dispatcher. The string parsing happened. It's done. It's gone. Only the structured result remains.

### A Compile-Time Expression Evaluator

Let's go further. Here's a constexpr evaluator for arithmetic expressions like `"3+4*2"`:

```cpp
constexpr int parse_number(std::string_view sv, std::size_t& pos) {
    int result = 0;
    while (pos < sv.size() && sv[pos] >= '0' && sv[pos] <= '9') {
        result = result * 10 + (sv[pos] - '0');
        ++pos;
    }
    return result;
}

constexpr int parse_factor(std::string_view sv, std::size_t& pos) {
    return parse_number(sv, pos);
}

constexpr int parse_term(std::string_view sv, std::size_t& pos) {
    int left = parse_factor(sv, pos);
    while (pos < sv.size() && sv[pos] == '*') {
        ++pos;
        left *= parse_factor(sv, pos);
    }
    return left;
}

constexpr int parse_expr(std::string_view sv, std::size_t& pos) {
    int left = parse_term(sv, pos);
    while (pos < sv.size() && (sv[pos] == '+' || sv[pos] == '-')) {
        char op = sv[pos++];
        int right = parse_term(sv, pos);
        left = (op == '+') ? left + right : left - right;
    }
    return left;
}

constexpr int evaluate(std::string_view expr) {
    std::size_t pos = 0;
    return parse_expr(expr, pos);
}

static_assert(evaluate("3+4*2") == 11);
static_assert(evaluate("10-2*3") == 4);
static_assert(evaluate("100+200+300") == 600);
```

That's a recursive descent parser. With operator precedence. Running at compile time. The `static_assert` *proves* that the compiler evaluated `"3+4*2"` and got `11`. No runtime code was generated. The parser exists only inside the compiler.

And the technique scales. People have written compile-time JSON parsers, compile-time SQL validators, compile-time regex engines. The constexpr evaluator in modern compilers is powerful enough to run substantial parsing algorithms.

## CTRE: The Crown Jewel

The most impressive application of compile-time string parsing is CTRE — Hana Dusikova's Compile-Time Regular Expressions library. You write:

```cpp
auto match = ctre::match<"([0-9]{4})-([0-9]{2})-([0-9]{2})">(date_string);
```

The regex pattern is a `fixed_string` NTTP. CTRE parses it at compile time into an AST, transforms the AST into a type-level state machine, and the compiler generates optimal matching code — no regex interpretation, no NFA simulation, no backtracking engine. Just straight-line comparison code that the optimizer can inline and vectorize.

The performance is competitive with *hand-written parsing code*. The regex syntax gives you the expressiveness of a pattern language. And it's all validated at compile time — a malformed regex is a compile error, not a runtime exception.

This is the endgame of compile-time string processing. Take a domain-specific notation, parse it during compilation, and emit optimized code specialized for that exact pattern. The string literal is a program in a mini-language. The template machinery is its compiler. The output is machine code.

## The Pattern

Every compile-time string processing application follows the same three-step pattern:

1. **Accept** a string literal as a `fixed_string` NTTP
2. **Parse** it in a constexpr function, producing structured data
3. **Use** that structured data to drive template instantiation, `static_assert` validation, or code generation

The string is a program. The constexpr parser is its compiler. The output is types and values that the C++ optimizer can work with. By the time the binary is emitted, the string is gone — only its meaning remains, encoded in optimized machine code.

This is a fundamental shift. Instead of processing strings at runtime and *hoping* they're well-formed, you process them at compile time and *guarantee* correctness. The compiler becomes a parser for your domain-specific notation. And your domain-specific notation compiles to zero-overhead code.

Strings gave the compile-time language the ability to read. What it does with that ability is limited only by your imagination — and your patience for constexpr debugging.
