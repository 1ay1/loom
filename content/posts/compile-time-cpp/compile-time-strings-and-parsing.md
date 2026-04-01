---
title: "Compile-Time Strings and Parsing — Reading Text Inside the Compiler"
date: 2026-02-19
slug: compile-time-strings-and-parsing
tags: compile-time-cpp, fixed-string, constexpr-parsing, nttp, string-literals
excerpt: String literals live at compile time. With the right tools, you can parse them, validate them, and turn them into optimized code — all before your program runs.
---

String literals are compile-time data. The characters of `"hello"` are baked into the binary by the compiler — they exist, fully formed, during compilation. Yet for most of C++'s history, templates couldn't touch them. You could pass integers as template arguments, enumerators, even pointers to global objects, but not strings. The type system could reason about numbers and types but was blind to text.

C++20 changed this. Class-type non-type template parameters (NTTPs) opened the door to passing string literals as template arguments, and the consequences are profound. Once strings are visible to the template machinery, you can parse them, validate them, decompose them into structured data, and generate optimized code — all at compile time. Format string validation, URL route parsing, compile-time regex, embedded DSLs. The entire category of "text in, code out" transformations becomes available without code generation tools or macros.

This post covers the mechanics: why strings were off-limits, how to make them work, and what you can do once they're available.

## The Problem with String Literals and Templates

A string literal `"hello"` has type `const char[6]` (five characters plus the null terminator). It's an array. And before C++20, arrays couldn't be non-type template parameters.

The rules for NTTPs were strict. You could use integral types, enumerations, pointers, references, and (since C++17) `auto` to deduce from those categories. But arrays and class types were out. This meant you couldn't write:

```cpp
template<const char* S>  // legal, but almost useless
struct message {};

message<"hello"> m;  // error: string literal can't bind to NTTP pointer
```

The pointer form `template<const char* S>` is technically allowed, but the argument must be a pointer to an object with external linkage. You can't pass a string literal directly. You'd need to declare a global:

```cpp
constexpr char hello[] = "hello";
message<hello> m;  // OK in C++17, but painful
```

This works but defeats the purpose. The ergonomics are terrible. Every distinct string needs a global variable. You can't write generic code that accepts arbitrary string literals. The compile-time language can see numbers but not text.

## fixed_string — The Bridge

C++20 allows class types as NTTPs, provided the class is a *structural type*: all bases and non-static data members are public, and the type has no mutable or volatile members. A simple struct wrapping a `char` array qualifies.

```cpp
template<std::size_t N>
struct fixed_string {
    char data[N];

    constexpr fixed_string(const char (&str)[N]) {
        std::copy_n(str, N, data);
    }

    constexpr std::size_t size() const { return N - 1; } // exclude null terminator

    constexpr char operator[](std::size_t i) const { return data[i]; }

    constexpr bool operator==(const fixed_string&) const = default;

    constexpr operator std::string_view() const {
        return {data, N - 1};
    }
};

template<std::size_t N>
fixed_string(const char (&)[N]) -> fixed_string<N>;
```

The deduction guide is essential. It lets the compiler deduce `N` from the literal, so you never specify the size manually. Now you can write:

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

Each distinct string literal produces a distinct template instantiation. `greeting<"hello">` and `greeting<"world">` are different types. The compiler can see the full content of the string at template instantiation time, which means you can branch on it, parse it, and transform it — all during compilation.

## Compile-Time String Operations

Since `fixed_string` is a literal type and its constructor is constexpr, you can write constexpr functions that operate on it. The full toolkit of string manipulation is available at compile time.

**Find a character:**

```cpp
template<std::size_t N>
constexpr std::size_t find_char(const fixed_string<N>& s, char c, std::size_t pos = 0) {
    for (std::size_t i = pos; i < s.size(); ++i) {
        if (s[i] == c) return i;
    }
    return std::size_t(-1);  // npos
}
```

**Check a prefix:**

```cpp
template<std::size_t N, std::size_t M>
constexpr bool starts_with(const fixed_string<N>& haystack, const fixed_string<M>& prefix) {
    if (prefix.size() > haystack.size()) return false;
    for (std::size_t i = 0; i < prefix.size(); ++i) {
        if (haystack[i] != prefix[i]) return false;
    }
    return true;
}
```

**Concatenation** is trickier because the result has a different size, which means a different type. You need to return a `fixed_string<N + M - 1>` (both inputs include a null terminator, the result needs only one):

```cpp
template<std::size_t N, std::size_t M>
constexpr auto concat(const fixed_string<N>& a, const fixed_string<M>& b) {
    fixed_string<N + M - 1> result{};
    std::copy_n(a.data, N - 1, result.data);
    std::copy_n(b.data, M, result.data + N - 1);
    return result;
}
```

This requires adding a default constructor to `fixed_string`, or a constructor from a size tag. The exact details vary, but the point is that every operation you'd normally do with runtime strings has a constexpr counterpart. The compiler evaluates it all during compilation. The result is a new type encoding the computed string.

## Compile-Time Parsing

String operations are useful, but the real payoff is *parsing* — reading a string and producing structured data from it. This is where compile-time strings turn from a convenience into a programming model.

### Format String Validation

`std::format` validates its format string at compile time. If you write `std::format("{} is {} years old", name)`, the compiler rejects it because there are two placeholders but only one argument. How?

The core idea is a constexpr function that counts placeholders:

```cpp
template<fixed_string Fmt>
constexpr std::size_t count_placeholders() {
    std::size_t count = 0;
    for (std::size_t i = 0; i < Fmt.size(); ++i) {
        if (Fmt[i] == '{') {
            if (i + 1 < Fmt.size() && Fmt[i + 1] == '}') {
                ++count;
                ++i;  // skip the '}'
            }
        }
    }
    return count;
}
```

Now you can enforce the count at compile time:

```cpp
template<fixed_string Fmt, typename... Args>
void checked_print(Args&&... args) {
    static_assert(count_placeholders<Fmt>() == sizeof...(Args),
        "format string placeholder count doesn't match argument count");
    // actual formatting logic...
}

checked_print<"{} is {} years old">("Alice", 30);  // OK
checked_print<"{} is {} years old">("Alice");       // compile error
```

The format string is parsed during compilation. The `static_assert` fires if the counts don't match. No runtime cost. No possibility of a format string mismatch reaching production. This is the pattern `std::format` uses (with considerably more sophistication — it also validates type specifiers, width, precision, and so on).

### Route Parsing

Web frameworks need to parse URL patterns like `"/users/{id}/posts/{post_id}"` and extract parameter names. Doing this at compile time means the router can be a zero-overhead dispatch table.

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

The entire URL pattern is parsed during compilation. At runtime, the code sees `route.param_count == 2` as a compile-time constant. The compiler can unroll loops, eliminate branches, and inline the parameter extraction logic. The string parsing vanishes.

### Simple Expression Evaluator

For a more complete example, here's a constexpr evaluator for simple arithmetic expressions like `"3+4*2"`:

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
```

This is a standard recursive descent parser. The only thing unusual about it is that it runs at compile time. The `static_assert` proves that the compiler evaluated `"3+4*2"` and got `11`. No runtime code was generated. The parser exists only inside the compiler.

You can scale this approach up. People have written compile-time JSON parsers, compile-time SQL parsers, compile-time regex engines. The constexpr evaluation engine in modern C++ compilers is powerful enough to run substantial parsing algorithms.

## constexpr string_view

You may have noticed the expression parser above uses `std::string_view` rather than `fixed_string`. That's deliberate. `string_view` is a pointer and a length — it's lightweight, it supports all the substring operations you'd want, and it's fully constexpr since C++17.

The distinction between `string_view` and `fixed_string` is important:

- **`fixed_string`** is a value type that owns its characters. It can be an NTTP. Use it to pass string literals into templates.
- **`string_view`** is a reference type. It can't be an NTTP (it contains a pointer, which doesn't satisfy the structural type requirements in a useful way). Use it inside constexpr functions to manipulate strings without copying.

The typical pattern is: accept the string as a `fixed_string` NTTP, convert it to a `string_view` inside the constexpr function, do your parsing, and return structured results.

```cpp
template<fixed_string S>
constexpr auto parse() {
    std::string_view sv(S.data, S.size());
    // parse sv, return structured result
}
```

## Compile-Time Regex

The most impressive application of compile-time string parsing is CTRE — Hana Dusíková's Compile-Time Regular Expressions library. You write:

```cpp
auto match = ctre::match<"([0-9]{4})-([0-9]{2})-([0-9]{2})">(date_string);
```

The regex pattern is a `fixed_string` NTTP. CTRE parses it at compile time into an AST, then transforms the AST into a type-level state machine. The compiler generates optimal matching code — no regex interpretation, no NFA simulation, no backtracking engine. Just straight-line comparison code that the optimizer can inline and vectorize.

The performance is competitive with hand-written parsing code. The regex syntax gives you the expressiveness of a pattern language. And it's all validated at compile time — a malformed regex is a compile error, not a runtime exception.

This is the endgame of compile-time string processing: take a domain-specific notation, parse it during compilation, and emit optimized code that's specialized for that exact pattern. The string literal is a program in a mini-language. The template machinery is its compiler.

## Limitations and Workarounds

Compile-time strings have sharp edges.

**constexpr allocation is transient.** Since C++20, you can use `std::string` and `std::vector` in constexpr contexts. You can build a `std::string`, manipulate it, concatenate, search — all at compile time. But the allocation must not *escape* the constexpr evaluation. You can't have a `constexpr std::string` as a global variable, because the heap memory would need to exist at runtime, and the compiler doesn't emit heap data into the binary.

```cpp
constexpr auto process() {
    std::string s = "hello";
    s += " world";
    return s.size();  // OK: returning an int, not the string
}

constexpr auto len = process();  // fine, len == 11

// constexpr std::string greeting = "hello";  // error: allocation escapes
```

The workaround is to use transient `std::string` during your compile-time computation but return non-allocating types — integers, `fixed_string`, arrays, structs with fixed-size members. Parse with `std::string` for convenience, package the results into a structural type.

**String size is part of the type.** `fixed_string<6>` and `fixed_string<7>` are different types. You can't have a function that returns a `fixed_string` of data-dependent length (at least not easily). If your parsing produces strings of different lengths, you need to either pad to a maximum size or use a different representation for the output.

**Compiler limits on constexpr steps.** Parsing a long string might hit the compiler's constexpr evaluation step limit. GCC defaults to 33 million steps (`-fconstexpr-ops-limit`), Clang to one million (`-fconstexpr-steps`). For most practical format strings and patterns this is fine, but parsing a large embedded DSL might require raising the limit.

**Debuggability.** When a constexpr parse fails, you get a compiler error. Compiler errors from deep constexpr evaluation can be difficult to read. Strategic use of `static_assert` with descriptive messages helps, but the experience is nowhere near as smooth as debugging runtime code. Some developers add constexpr validation functions that produce clear `static_assert` failures for each possible parse error, rather than letting the compiler report whatever internal failure it hits first.

## The Pattern

The compile-time string processing pattern is always the same:

1. Accept a string literal as a `fixed_string` NTTP.
2. Parse it in a constexpr function, producing structured data.
3. Use that structured data to drive template instantiation, `static_assert` validation, or code generation.

The string is a program. The constexpr parser is its compiler. The output is types and values that the C++ compiler's normal optimization passes can work with. By the time the binary is emitted, the string is gone — only its meaning remains, encoded in optimized machine code.

This is a fundamental shift from how C++ traditionally handled text. Instead of processing strings at runtime and hoping they're well-formed, you process them at compile time and *guarantee* correctness. The compiler becomes a parser for your domain-specific notation, and your domain-specific notation compiles to zero-overhead code.
