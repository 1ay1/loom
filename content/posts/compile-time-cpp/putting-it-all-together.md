---
title: "Putting It All Together — Building a Compile-Time DSL from Scratch"
date: 2026-03-01
slug: putting-it-all-together
tags: compile-time-cpp, dsl, metaprogramming, compile-time-dsl, type-safe
excerpt: Twelve posts of theory. Now we build. A complete, type-safe, zero-overhead domain-specific language — using every tool the compile-time language has taught us.
---

Twelve posts. We've covered types as values, templates as functions, branching, iteration, pattern matching, data structures, higher-order programming, string parsing, error messages, constexpr evaluation, concepts, and reflection. That's a lot of tools. The question you should be asking: what can you build with all of them?

The answer: a compile-time DSL. A domain-specific language embedded inside C++, parsed and validated during compilation, that generates optimal code with zero runtime overhead. The DSL source lives in your C++ code as string literals or type expressions. The DSL compiler lives inside the C++ compiler. The output is specialized machine code, tailored to your exact domain logic, with every error caught before the program runs.

This post builds one from scratch. We'll design a type-safe query language for in-memory data — think a baby SQL, embedded in C++, fully validated at compile time, compiled to tight loops. Along the way, we'll use nearly every technique from the series. This is the capstone.

## The Goal

We want to write queries like this:

```cpp
auto results = query<"SELECT name, age FROM people WHERE age > 30 ORDER BY name">(data);
```

And have the compiler:

1. **Parse** the query string at compile time
2. **Validate** that the referenced columns exist in the data type
3. **Type-check** that the comparisons make sense (no comparing strings to integers)
4. **Generate** optimized code — a tight loop with direct member access, no string lookups, no runtime parsing
5. **Reject** malformed queries with clear compile-time error messages

The runtime cost should be equivalent to hand-written C++ that directly accesses struct members in a loop. The query string is syntactic sugar that dissolves during compilation.

## Step 1: The Data Model

First, we need a way to describe the data we're querying. A struct with some metadata:

```cpp
struct Person {
    std::string name;
    int age;
    std::string email;
    double salary;
};
```

To make this queryable, we need to associate column names with member pointers. Pre-reflection, this requires a registration step. With C++26 reflection, it's automatic — but let's do it both ways.

**Manual registration (works today):**

```cpp
template<typename T>
struct table_traits;

template<>
struct table_traits<Person> {
    static constexpr auto columns = std::make_tuple(
        column{"name",   &Person::name},
        column{"age",    &Person::age},
        column{"email",  &Person::email},
        column{"salary", &Person::salary}
    );
    static constexpr auto table_name = "people";
};
```

Where `column` is a simple wrapper:

```cpp
template<typename T, typename Class>
struct column {
    fixed_string<32> name;
    T Class::* member;

    constexpr column(const char* n, T Class::* m) : name{}, member(m) {
        for (int i = 0; n[i]; ++i) name.data[i] = n[i];
    }
};

// Deduction guide
template<std::size_t N, typename T, typename Class>
column(const char (&)[N], T Class::*) -> column<T, Class>;
```

**Automatic registration (C++26 reflection):**

```cpp
template<typename T>
consteval auto make_table_traits() {
    constexpr auto members = std::meta::nonstatic_data_members_of(^^T);
    // Build column descriptors from reflected member info
    return /* tuple of columns derived from reflection */;
}
```

For this post, we'll use the manual registration approach since it works with current compilers. The reflection version replaces the boilerplate but doesn't change the design.

## Step 2: Parsing the Query String

The query string `"SELECT name, age FROM people WHERE age > 30 ORDER BY name"` needs to be parsed at compile time into a structured representation. We built the tools for this in [Compile-Time Strings and Parsing](/post/compile-time-strings-and-parsing).

First, the parsed query structure:

```cpp
enum class CompOp { Eq, Ne, Lt, Le, Gt, Ge };

struct parsed_condition {
    fixed_string<64> column_name;
    CompOp op;
    int64_t int_value;      // for numeric comparisons
    bool has_string_value;
    fixed_string<64> string_value;
};

template<std::size_t MaxCols = 8>
struct parsed_query {
    fixed_string<64> table_name;
    fixed_string<64> selected_columns[MaxCols];
    std::size_t num_selected = 0;

    bool has_where = false;
    parsed_condition where_condition;

    bool has_order_by = false;
    fixed_string<64> order_by_column;
    bool order_descending = false;
};
```

The parser is a constexpr function that reads the query string and populates this struct:

```cpp
constexpr auto skip_whitespace(std::string_view sv, std::size_t pos) {
    while (pos < sv.size() && sv[pos] == ' ') ++pos;
    return pos;
}

constexpr auto read_word(std::string_view sv, std::size_t pos) {
    auto start = pos;
    while (pos < sv.size() && sv[pos] != ' ' && sv[pos] != ',') ++pos;
    return std::pair{sv.substr(start, pos - start), pos};
}

constexpr auto read_number(std::string_view sv, std::size_t pos) {
    bool negative = false;
    if (pos < sv.size() && sv[pos] == '-') {
        negative = true;
        ++pos;
    }
    int64_t result = 0;
    while (pos < sv.size() && sv[pos] >= '0' && sv[pos] <= '9') {
        result = result * 10 + (sv[pos] - '0');
        ++pos;
    }
    return std::pair{negative ? -result : result, pos};
}

template<fixed_string Query>
consteval auto parse_query() {
    constexpr std::string_view sv{Query.data, Query.size()};
    parsed_query result{};
    std::size_t pos = 0;

    // Parse SELECT
    pos = skip_whitespace(sv, pos);
    auto [select_kw, pos2] = read_word(sv, pos);
    // (assert select_kw == "SELECT")
    pos = skip_whitespace(sv, pos2);

    // Parse column list
    while (true) {
        auto [col, next_pos] = read_word(sv, pos);
        // Copy col into result.selected_columns[result.num_selected++]
        auto& target = result.selected_columns[result.num_selected++];
        for (std::size_t i = 0; i < col.size(); ++i) {
            target.data[i] = col[i];
        }
        pos = skip_whitespace(sv, next_pos);
        if (pos < sv.size() && sv[pos] == ',') {
            ++pos;
            pos = skip_whitespace(sv, pos);
        } else {
            break;
        }
    }

    // Parse FROM
    auto [from_kw, pos3] = read_word(sv, pos);
    pos = skip_whitespace(sv, pos3);
    auto [table, pos4] = read_word(sv, pos);
    for (std::size_t i = 0; i < table.size(); ++i) {
        result.table_name.data[i] = table[i];
    }
    pos = skip_whitespace(sv, pos4);

    // Parse optional WHERE
    if (pos < sv.size()) {
        auto [kw, pos5] = read_word(sv, pos);
        if (kw == "WHERE") {
            result.has_where = true;
            pos = skip_whitespace(sv, pos5);

            auto [wcol, pos6] = read_word(sv, pos);
            for (std::size_t i = 0; i < wcol.size(); ++i)
                result.where_condition.column_name.data[i] = wcol[i];
            pos = skip_whitespace(sv, pos6);

            auto [op, pos7] = read_word(sv, pos);
            if (op == ">")  result.where_condition.op = CompOp::Gt;
            if (op == "<")  result.where_condition.op = CompOp::Lt;
            if (op == ">=") result.where_condition.op = CompOp::Ge;
            if (op == "<=") result.where_condition.op = CompOp::Le;
            if (op == "=")  result.where_condition.op = CompOp::Eq;
            if (op == "!=") result.where_condition.op = CompOp::Ne;
            pos = skip_whitespace(sv, pos7);

            auto [val, pos8] = read_number(sv, pos);
            result.where_condition.int_value = val;
            pos = skip_whitespace(sv, pos8);
        }
    }

    // Parse optional ORDER BY
    if (pos < sv.size()) {
        auto [kw1, pos9] = read_word(sv, pos);
        if (kw1 == "ORDER") {
            pos = skip_whitespace(sv, pos9);
            auto [kw2, pos10] = read_word(sv, pos);
            // (assert kw2 == "BY")
            pos = skip_whitespace(sv, pos10);
            auto [ocol, pos11] = read_word(sv, pos);
            for (std::size_t i = 0; i < ocol.size(); ++i)
                result.order_by_column.data[i] = ocol[i];
            result.has_order_by = true;
        }
    }

    return result;
}
```

This is a straightforward recursive descent parser — the same kind we wrote in the strings post, just bigger. Every string comparison, every index increment, every conditional — all of it runs at compile time. The result is a `parsed_query` value embedded in the binary as constant data.

## Step 3: Validation at Compile Time

The parsed query needs validation. Do the referenced columns exist? Are the comparisons type-safe? Is the table name correct? This is where `static_assert` and `consteval` earn their keep.

```cpp
template<typename T>
consteval bool column_exists(std::string_view name) {
    constexpr auto& cols = table_traits<T>::columns;
    return std::apply([&](auto&... col) {
        return ((std::string_view(col.name.data) == name) || ...);
    }, cols);
}

template<typename T, fixed_string Query>
consteval void validate_query() {
    constexpr auto q = parse_query<Query>();

    // Check all selected columns exist
    for (std::size_t i = 0; i < q.num_selected; ++i) {
        if (!column_exists<T>(q.selected_columns[i].data)) {
            throw "Selected column does not exist in table";
        }
    }

    // Check WHERE column exists
    if (q.has_where) {
        if (!column_exists<T>(q.where_condition.column_name.data)) {
            throw "WHERE column does not exist in table";
        }
    }

    // Check ORDER BY column exists
    if (q.has_order_by) {
        if (!column_exists<T>(q.order_by_column.data)) {
            throw "ORDER BY column does not exist in table";
        }
    }
}
```

If any validation fails, the `throw` statement fires at compile time — which means it becomes a compile error. You see "Selected column does not exist in table," not a runtime exception. The malformed query is rejected before the program is built.

This is the [Error Messages and Diagnostics](/post/error-messages-and-diagnostics) pattern applied to a DSL. The compile-time language validates the domain-specific language. Invalid queries are type errors.

## Step 4: Code Generation — The Compile-Time Dispatch

Now the interesting part. We need to turn a parsed query into executable code. The parsed query has column names as strings. The actual data has members accessed through member pointers. We need to bridge the gap — at compile time.

The key technique is **compile-time string matching** against the column registry. For each column name in the query, we find the corresponding member pointer at compile time, and generate code that uses it directly.

```cpp
template<typename T, fixed_string ColName>
constexpr auto get_member_pointer() {
    constexpr auto& cols = table_traits<T>::columns;
    // Find the column whose name matches ColName
    // Return its member pointer
    return std::apply([](auto&... col) {
        auto result = /* find matching column */;
        return result;
    }, cols);
}
```

The actual dispatch uses `if constexpr` chains or — more elegantly — `std::apply` with a fold expression to iterate the column tuple and match names at compile time:

```cpp
template<typename T, typename F>
constexpr void with_column(std::string_view name, const T& row, F&& func) {
    std::apply([&](auto&... col) {
        ((std::string_view(col.name.data) == name ?
            (func(row.*(col.member)), true) : false) || ...);
    }, table_traits<T>::columns);
}
```

This fold expression tries each column. When it finds the one whose name matches, it accesses the member through the member pointer and calls `func` with the value. The short-circuit `||` stops at the first match. Since the name comparison happens at compile time (when the query is a constant), the optimizer eliminates all the comparisons and produces a direct member access.

## Step 5: The WHERE Clause

The WHERE clause needs to compare a column's value against a constant. The comparison operator and the column are both known at compile time. Only the data varies at runtime.

```cpp
template<typename T, CompOp Op>
constexpr bool compare(const T& value, int64_t rhs) {
    if constexpr (Op == CompOp::Gt) return value > rhs;
    if constexpr (Op == CompOp::Lt) return value < rhs;
    if constexpr (Op == CompOp::Ge) return value >= rhs;
    if constexpr (Op == CompOp::Le) return value <= rhs;
    if constexpr (Op == CompOp::Eq) return value == rhs;
    if constexpr (Op == CompOp::Ne) return value != rhs;
}
```

Each `if constexpr` branch compiles to a single comparison instruction. The branch selection happens at compile time. The runtime code is just the comparison that matched.

## Step 6: Putting It Together

The final `query` function template ties everything together:

```cpp
template<fixed_string Query, typename T>
auto query(const std::vector<T>& data) {
    // Step 1: Parse at compile time
    constexpr auto q = parse_query<Query>();

    // Step 2: Validate at compile time
    validate_query<T, Query>();

    // Step 3: Execute at runtime with compile-time-optimized code
    std::vector<T> results;

    // Apply WHERE filter
    for (const auto& row : data) {
        bool passes = true;
        if constexpr (q.has_where) {
            with_column(
                std::string_view(q.where_condition.column_name.data),
                row,
                [&](const auto& val) {
                    passes = compare<std::remove_cvref_t<decltype(val)>,
                                     q.where_condition.op>(val, q.where_condition.int_value);
                }
            );
        }
        if (passes) {
            results.push_back(row);
        }
    }

    // Apply ORDER BY
    if constexpr (q.has_order_by) {
        std::sort(results.begin(), results.end(),
            [](const T& a, const T& b) {
                bool result = false;
                with_column(
                    std::string_view(q.order_by_column.data),
                    a,
                    [&](const auto& val_a) {
                        with_column(
                            std::string_view(q.order_by_column.data),
                            b,
                            [&](const auto& val_b) {
                                result = val_a < val_b;
                            }
                        );
                    }
                );
                return result;
            }
        );
    }

    return results;
}
```

Let's trace what happens when the compiler sees:

```cpp
auto results = query<"SELECT name, age FROM people WHERE age > 30 ORDER BY name">(data);
```

1. **Template instantiation**: `Query` is bound to the fixed_string `"SELECT name, age FROM people WHERE age > 30 ORDER BY name"`.

2. **Parse**: `parse_query<Query>()` runs at compile time. The string is decomposed into a `parsed_query` struct: two selected columns (`name`, `age`), table `people`, WHERE condition (`age > 30`), ORDER BY (`name`).

3. **Validate**: `validate_query<Person, Query>()` checks that `name`, `age` exist in `table_traits<Person>`. They do. If someone had written `WHERE height > 30`, compilation would fail with "WHERE column does not exist in table."

4. **Code generation**: The `if constexpr` branches are resolved. Since `q.has_where` is `true` (compile-time constant), the WHERE filtering code is included. Since `q.has_order_by` is `true`, the ORDER BY code is included.

5. **Member access**: `with_column` matches `"age"` against the column registry, finds `&Person::age`, and generates `row.age > 30` as the comparison. No string comparison at runtime. No hash table lookup. Just direct member access.

6. **Optimization**: The compiler sees `row.age > 30` (a simple integer comparison) and `std::sort` with a comparator that does `a.name < b.name` (a string comparison). All the template machinery, all the constexpr computation, all the fold expressions — dissolved. The generated code is indistinguishable from what you'd write by hand.

## What the Compiler Actually Generates

After optimization, the compiled code for our query looks approximately like:

```cpp
// This is what the optimizer produces — not what you write
std::vector<Person> results;
for (const auto& row : data) {
    if (row.age > 30) {
        results.push_back(row);
    }
}
std::sort(results.begin(), results.end(),
    [](const Person& a, const Person& b) { return a.name < b.name; });
```

That's it. A loop, a comparison, a push_back, a sort. No parsing. No string matching. No virtual dispatch. No interpretation. The query string was a program in a domain-specific language. The C++ compiler compiled it into optimal machine code.

The abstraction is complete: the DSL exists in the source code, the optimized code exists in the binary, and nothing exists in between.

## Techniques Used

Let's inventory what we used from the series:

| Technique | From Post | Used For |
|-----------|----------|----------|
| Types as values | [Post 2](/post/types-as-values) | Member pointer types driving code generation |
| `if constexpr` | [Post 3](/post/control-flow-at-compile-time) | Compile-time branch selection for WHERE/ORDER BY |
| Fold expressions | [Post 4](/post/iteration-at-compile-time) | Iterating column registry at compile time |
| Pattern matching | [Post 5](/post/pattern-matching-at-compile-time) | `table_traits` specialization per type |
| Type lists / tuples | [Post 6](/post/data-structures-at-compile-time) | Column registry as `std::tuple` |
| Higher-order functions | [Post 7](/post/metafunctions-and-higher-order-programming) | `with_column` taking a lambda as callback |
| Compile-time strings | [Post 8](/post/compile-time-strings-and-parsing) | `fixed_string` NTTP for query input, constexpr parser |
| Error diagnostics | [Post 9](/post/error-messages-and-diagnostics) | `consteval` validation with `throw` for clear errors |
| constexpr/consteval | [Post 10](/post/constexpr-everything) | Parser and validator running inside the compiler |
| Concepts | [Post 11](/post/concepts-as-interfaces) | Constraining queryable types (not shown but trivial to add) |
| Reflection | [Post 12](/post/reflection-and-code-generation) | Automatic column registration (C++26 path) |

Twelve posts. One DSL. Every technique contributed.

## Extending the DSL

The beauty of compile-time DSLs is that extensions are just more compile-time code. Want aggregate functions?

```cpp
auto total = query<"SELECT SUM(salary) FROM people WHERE age > 30">(data);
```

Parse `SUM(salary)` as an aggregate operation. At compile time, generate a loop that accumulates `row.salary` instead of collecting rows. The optimizer sees a simple accumulation loop.

Want joins?

```cpp
auto result = query<"SELECT p.name, d.name FROM people p JOIN departments d ON p.dept_id = d.id">(people, departments);
```

Parse the join syntax. At compile time, validate that `dept_id` exists in `Person` and `id` exists in `Department`. Generate nested loops (or hash-join code if you're feeling ambitious). The join strategy is chosen at compile time based on the types involved.

Want type-safe projections that return only the selected columns?

```cpp
// Returns std::vector<std::tuple<std::string, int>> instead of std::vector<Person>
auto results = query<"SELECT name, age FROM people">(data);
```

Generate a result type at compile time using the selected columns' types. The return type of `query` changes based on the SELECT clause. The caller gets a tuple of exactly the requested columns, and the compiler type-checks all downstream usage.

Each extension follows the same pattern: parse at compile time, validate at compile time, generate code at compile time, pay for nothing at runtime.

## The Limits

This approach has real limits, and it's worth being honest about them.

**The queries must be compile-time constants.** You can't write `query<user_input>(data)` — the query string must be a literal. Dynamic queries still need runtime parsing. (Though you could constexpr-compile a set of known queries and dispatch to them at runtime.)

**Compilation gets slower.** Every constexpr computation adds compilation time. A complex parser evaluating hundreds of queries can measurably slow your build. This is the fundamental tradeoff: you're moving work from runtime to compile time. That work doesn't disappear — it just happens at a different time.

**Error messages need care.** A raw `throw` in a `consteval` function produces a compiler error, but the message might be buried in template instantiation context. You need to design your error messages deliberately, as we discussed in [Diagnostics](/post/error-messages-and-diagnostics).

**Debugging is different.** You can't step through constexpr evaluation in most debuggers (yet). When your compile-time parser has a bug, you debug by inspecting `static_assert` outputs and intermediate constexpr values. It's not as smooth as runtime debugging.

**Complexity budget.** Not every string literal needs to be a DSL. Not every configuration needs compile-time validation. The cost of building and maintaining a compile-time DSL is real, and it's only justified when the benefits — type safety, zero-overhead, early error detection — are worth it.

## The Mental Model, Final Form

This series started with a claim: C++ is two languages. A runtime language and a compile-time language. The compile-time language has values (types), functions (templates), branching, iteration, data structures, higher-order programming, a type system, introspection, and now code generation.

Here is the complete picture:

The compile-time language is a **metaprogramming system** — a language for writing programs that write programs. Your C++ source code is simultaneously a runtime program (the code that will execute on hardware) and a compile-time program (the code that generates, validates, and optimizes the runtime program).

When you write a `constexpr` function, you're writing code that runs twice: once in the compiler, once at runtime (if needed). When you write a template, you're writing a function that generates code. When you write a concept, you're writing a type in the compile-time language's type system. When you parse a string literal at compile time, you're building a compiler inside the compiler. When you splice reflection results, you're writing code that writes code that writes machine code.

This is what we've been building toward. Not tricks. Not dark magic. A coherent programming language that runs during compilation, generates optimal runtime code, and catches errors before the program exists.

The compile-time language started as an accident — template Turing-completeness discovered by happenstance in 1994. Thirty years later, it's a deliberate, designed, evolving programming system. It has its own idioms, its own standard library, its own type system, and its own execution model. It's not the prettiest language. The syntax carries decades of backward compatibility. The error messages are getting better but still have far to go.

But it works. It generates the fastest code. It catches the most bugs. It enables abstractions that dissolve completely. And now you speak it.
