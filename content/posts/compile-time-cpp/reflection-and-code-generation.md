---
title: "Reflection and Code Generation — The Compile-Time Language Grows Eyes"
date: 2026-02-27
slug: reflection-and-code-generation
tags: compile-time-cpp, reflection, code-generation, cpp26, metaclasses
excerpt: The compile-time language has been blind — it can compute with types but can't look inside them. C++26 reflection changes that. The compiler is about to hand you the keys to the AST.
---

For eleven posts, we've been programming in the compile-time language. We can manipulate types, iterate type lists, parse strings, validate invariants, compose metafunctions, and design concept hierarchies. But there's been a glaring hole the entire time: the compile-time language can't see inside types.

You can ask "is `T` a pointer?" (structural pattern matching). You can ask "does `T` have a `.size()` method?" (requires expression). But you cannot ask "what are the members of `T`?" You cannot ask "what are the names of `T`'s member variables?" You cannot ask "give me a list of all the methods on `T`, their names, their parameter types, their return types."

This is reflection. The ability of a program to examine its own structure. And C++ — a language that has had templates for three decades, constexpr for fifteen years, and concepts for six — has not had it. Until now.

C++26 introduces static reflection. It is, without exaggeration, the most significant addition to the compile-time language since templates themselves. This post covers what's coming, why it matters, and what you'll be able to do with it.

## The Reflection Gap

Consider a common task: serialize a struct to JSON.

```cpp
struct Person {
    std::string name;
    int age;
    std::string email;
};
```

In Python, you'd call `vars(person)` and get a dictionary. In Java, you'd use `person.getClass().getDeclaredFields()`. In C#, `typeof(Person).GetProperties()`. Every mainstream language has some way to enumerate a type's members at runtime.

In C++ before C++26, your options were:

1. **Write it by hand.** For each struct, write a serialize function that knows all the member names. Repeat for every struct. Repeat for every format (JSON, XML, binary, protobuf). Repeat for deserialization. Watch your code become 80% boilerplate.

2. **Macros.** Use a registration macro like `REFLECT(Person, name, age, email)` that generates the boilerplate through preprocessor expansion. This works but is fragile, ugly, and limited — macros can't see the types of members, can't handle templates, and produce inscrutable errors.

3. **Code generation.** Use an external tool (protobuf, flatbuffers, Cap'n Proto) that reads a schema definition and generates C++ code. This works well but requires a build system step, a separate schema language, and can't reflect over arbitrary C++ types.

4. **Describe it twice.** Use a library like Boost.PFR that can reflect over simple aggregates by exploiting structured bindings, but only works for aggregates with no user-declared constructors, no base classes, no private members — a severely limited subset.

All of these are workarounds for a missing language feature. C++26 provides the real thing.

## The Mirror

C++26 reflection is based on a simple idea: an expression `^^T` (read "reflect on T") produces a *reflection* — a compile-time value that describes `T`. You can then query this reflection using functions in `std::meta`.

```cpp
#include <meta>

struct Point {
    double x;
    double y;
    double z;
};

constexpr auto point_mirror = ^^Point;
```

`point_mirror` is a value of type `std::meta::info`. It's an opaque handle that represents the `Point` type in the compiler's internal data structures. You can't print it directly. But you can ask it questions.

```cpp
// How many data members does Point have?
constexpr auto members = std::meta::nonstatic_data_members_of(^^Point);
static_assert(members.size() == 3);
```

`nonstatic_data_members_of` returns a `std::vector<std::meta::info>` — a list of reflections, one per data member. Each member reflection knows its name, its type, its offset, its access specifier, and more.

```cpp
// What are their names?
static_assert(std::meta::identifier_of(members[0]) == "x");
static_assert(std::meta::identifier_of(members[1]) == "y");
static_assert(std::meta::identifier_of(members[2]) == "z");

// What are their types?
static_assert(std::meta::type_of(members[0]) == ^^double);
static_assert(std::meta::type_of(members[1]) == ^^double);
```

This is the feature the compile-time language has been missing. The ability to inspect a type's structure — programmatically, at compile time, without macros or external tools. The compiler already has this information. Reflection just exposes it.

## The Splice

Reflection turns code into data (a `meta::info` value). The inverse operation — turning data back into code — is called **splicing**, spelled `[:refl:]`:

```cpp
constexpr auto refl = ^^int;
[:refl:] x = 42;  // equivalent to: int x = 42;
```

The `[:refl:]` splice takes a reflection and inserts the thing it reflects into the code. If `refl` reflects a type, the splice produces that type. If `refl` reflects a variable, the splice produces a reference to that variable. If `refl` reflects a function, the splice produces a call.

This is where reflection becomes generative. You can iterate over a type's members and splice each one into an expression:

```cpp
template<typename T>
constexpr auto to_tuple(const T& obj) {
    constexpr auto members = std::meta::nonstatic_data_members_of(^^T);
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        return std::make_tuple(obj.[:members[Is]:]...);
    }(std::make_index_sequence<members.size()>{});
}

// to_tuple(Point{1.0, 2.0, 3.0}) returns std::tuple{1.0, 2.0, 3.0}
```

The key expression is `obj.[:members[Is]:]` — "access the member of `obj` reflected by `members[Is]`." This is member access where the member name isn't written in the source code. It's computed. The compiler looks up which member `members[Is]` refers to and generates the appropriate access.

## Practical Application: Automatic Serialization

Now let's build what we always wanted — a generic serializer that works with any struct, no macros, no code generation:

```cpp
template<typename T>
std::string to_json(const T& obj) {
    constexpr auto members = std::meta::nonstatic_data_members_of(^^T);
    std::string result = "{";
    bool first = true;

    [:expand(members):] >> [&]<auto mem> {
        if (!first) result += ", ";
        first = false;
        result += "\"";
        result += std::meta::identifier_of(mem);
        result += "\": ";
        result += to_json_value(obj.[:mem:]);
    };

    result += "}";
    return result;
}
```

The `[:expand(members):]` syntax iterates over the reflections at compile time, invoking the lambda once per member. Each invocation gets a different `mem` — a `constexpr` reflection of one data member. `identifier_of(mem)` gives the member name as a string. `obj.[:mem:]` accesses the member.

Call `to_json(Person{"Alice", 30, "alice@example.com"})` and you get:

```json
{"name": "Alice", "age": 30, "email": "alice@example.com"}
```

One function. Works with any struct. No registration. No macros. No per-type boilerplate. The compiler tells us what the struct contains, and we serialize it.

Deserialization follows the same pattern, in reverse — iterate over members, read the JSON keys, and splice the values into the right member.

## Enum Reflection

Enums have been one of C++'s most frustrating types. You define them, you use them, and then you need to convert one to a string and you're back to writing a switch statement by hand:

```cpp
enum class Color { Red, Green, Blue };

// The old way — write it yourself
std::string_view to_string(Color c) {
    switch (c) {
        case Color::Red:   return "Red";
        case Color::Green: return "Green";
        case Color::Blue:  return "Blue";
    }
}
```

Three cases. Three strings. Maintained by hand. If you add `Color::Yellow`, the compiler won't tell you (unless you enable `-Wswitch`, and even then it's a warning, not an error). This has spawned countless "magic enum" libraries that use compiler-specific tricks to hack around the problem.

C++26 reflection makes it trivial:

```cpp
template<typename E>
    requires std::is_enum_v<E>
constexpr std::string_view enum_to_string(E value) {
    constexpr auto enumerators = std::meta::enumerators_of(^^E);
    for (auto e : enumerators) {
        if ([:e:] == value) {
            return std::meta::identifier_of(e);
        }
    }
    return "unknown";
}

static_assert(enum_to_string(Color::Red) == "Red");
static_assert(enum_to_string(Color::Blue) == "Blue");
```

`enumerators_of(^^E)` returns a list of reflections of the enum's enumerators. `[:e:]` splices each one back into a value for comparison. `identifier_of(e)` gives the name. The entire function works for any enum type, and it's evaluated at compile time.

The reverse — string to enum — is equally straightforward:

```cpp
template<typename E>
    requires std::is_enum_v<E>
constexpr std::optional<E> enum_from_string(std::string_view name) {
    constexpr auto enumerators = std::meta::enumerators_of(^^E);
    for (auto e : enumerators) {
        if (std::meta::identifier_of(e) == name) {
            return [:e:];
        }
    }
    return std::nullopt;
}

static_assert(enum_from_string<Color>("Green") == Color::Green);
static_assert(enum_from_string<Color>("Purple") == std::nullopt);
```

Write once. Works for every enum. No macros. No code generation. No maintenance burden.

## Generating Code: Beyond Introspection

Reflection isn't just about looking at types. It's about generating code based on what you see. The combination of reflection (examining structure) and splicing (generating code from reflections) is essentially compile-time code generation — without leaving C++.

### Automatic Comparison Operators

```cpp
template<typename T>
constexpr bool reflected_equal(const T& a, const T& b) {
    constexpr auto members = std::meta::nonstatic_data_members_of(^^T);
    bool equal = true;
    [:expand(members):] >> [&]<auto mem> {
        equal = equal && (a.[:mem:] == b.[:mem:]);
    };
    return equal;
}
```

This generates an equality comparison that checks each member in order. For a struct with five members, it generates five comparisons. For a struct with fifty, fifty. The code adapts to the structure of the type.

### Automatic Hash Functions

```cpp
template<typename T>
std::size_t reflected_hash(const T& obj) {
    std::size_t seed = 0;
    constexpr auto members = std::meta::nonstatic_data_members_of(^^T);
    [:expand(members):] >> [&]<auto mem> {
        seed ^= std::hash<std::remove_cvref_t<decltype(obj.[:mem:])>>{}(obj.[:mem:])
                 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    };
    return seed;
}
```

A hash function that automatically combines hashes of all members. No manual `boost::hash_combine` chain. No forgetting to update the hash when you add a member. The reflection handles it.

### Struct-of-Arrays Transformation

This is one of the most compelling examples. Game engines and scientific computing often need to transform an "array of structs" (AoS) into a "struct of arrays" (SoA) for cache performance. With reflection, this can be automated:

```cpp
template<typename T>
struct SoA {
    // For each member of T, create a vector of that member's type
    [:expand(std::meta::nonstatic_data_members_of(^^T)):] >> [&]<auto mem> {
        std::vector<[:std::meta::type_of(mem):]>
            [:std::meta::identifier_of(mem):];
    };
};

// SoA<Point> is equivalent to:
// struct {
//     std::vector<double> x;
//     std::vector<double> y;
//     std::vector<double> z;
// };
```

The `SoA` template iterates over `T`'s members and generates a new struct where each member becomes a `std::vector` of that member's type. The member names are preserved. The transformation is completely automatic.

This is the kind of thing that previously required either hand-written boilerplate, preprocessor magic, or an external code generation tool. Reflection makes it a template.

## Token Injection and define_class

C++26 also includes facilities for defining entirely new types at compile time. `std::meta::define_class` takes a reflection of an incomplete type and a list of member descriptions, and completes the class definition:

```cpp
struct Empty;

consteval void build_point() {
    std::vector<std::meta::data_member_spec> members;
    members.push_back(std::meta::data_member_spec{
        .name = "x", .type = ^^double
    });
    members.push_back(std::meta::data_member_spec{
        .name = "y", .type = ^^double
    });
    std::meta::define_class(^^Empty, members);
}
```

After `build_point()` executes at compile time, `Empty` is no longer empty — it has two `double` members named `x` and `y`. The class was constructed programmatically, at compile time, from data.

This opens the door to **metaclasses** — types that generate types. You describe what you want at a high level, and a consteval function builds the actual class:

```cpp
template<typename SchemaType>
struct ORM_Table {
    // Reflect on the schema and generate columns, accessors,
    // SQL generation methods, etc.
};
```

The schema is a plain struct. The ORM reflects on it and generates a complete database interface class. Add a member to the schema, and the ORM generates a new column, a new accessor, updated SQL generation — all automatically, all at compile time, all type-safe.

## Reflection and the Rest of the Series

Reflection doesn't invalidate anything we've learned. It extends it.

**Type lists** (Post 6) can now be built from reflection results. Instead of manually listing `type_list<int, double, std::string>`, you can reflect a struct's member types into a type list automatically.

**Metafunctions** (Post 7) can now operate on reflections as values. A metafunction that takes a `meta::info` and returns transformed `meta::info` is a metafunction over the compiler's own AST.

**Concepts** (Post 11) can now be checked against reflected interfaces. You can verify at compile time that a reflected type satisfies a concept, before generating code that uses it.

**Compile-time strings** (Post 8) combine with reflection for name-based operations. Member names are compile-time strings. You can pattern-match on them, concatenate them, use them to generate accessor names or serialization keys.

**constexpr** (Post 10) is the execution model for all of this. Reflection queries are constexpr functions. Splicing is a constexpr operation. The compile-time interpreter that runs constexpr code is the same engine that runs reflection-based code generation.

Reflection is the capstone. Everything before it was building the execution engine. Reflection gives that engine eyes — the ability to see into the program's own structure and generate code based on what it finds.

## The Compile-Time Language, Complete

With reflection, the compile-time language finally has all the pieces of a general-purpose programming language:

| Capability | Mechanism |
|-----------|-----------|
| Values | Types, constants, reflections |
| Variables | `using`, `constexpr`, `consteval` |
| Functions | Templates, constexpr functions, consteval functions |
| Branching | `if constexpr`, concepts, specialization |
| Iteration | Pack expansion, fold expressions, constexpr loops |
| Data structures | Type lists, tuples, constexpr containers |
| Pattern matching | Partial specialization |
| Higher-order functions | Template template parameters, metafunction classes |
| Type system | Concepts |
| Introspection | `^^` reflection |
| Code generation | `[::]` splicing |

This isn't a toy language anymore. It's a complete metaprogramming system that can examine, transform, and generate C++ code — all within the compiler, all type-safe, all with zero runtime overhead.

## What Reflection Won't Do

Reflection is static. It operates on the compile-time structure of types as declared in source code. It does not:

- **Reflect on runtime state.** You can't ask "what's the dynamic type of this pointer" at compile time. Dynamic dispatch remains a runtime mechanism.

- **Modify existing types.** Reflection is read-only for existing types. You can examine `Point`'s members, but you can't add a member to `Point`. (You can create a *new* type that has `Point`'s members plus more, via `define_class`.)

- **Cross translation unit boundaries** beyond what's visible through headers. Reflection sees what the compiler sees — the declarations available at the point of use.

- **Replace runtime reflection entirely.** If you need to examine types loaded from a DLL at runtime, or inspect objects whose types aren't known until runtime, you still need RTTI or a custom reflection system. Compile-time reflection handles the cases where the types are known at compile time — which, in C++, is most cases.

## The Bigger Picture

For thirty years, C++ metaprogramming has been building a compile-time programming language, one feature at a time. Templates gave it functions. Specialization gave it pattern matching. Variadic templates gave it lists. `constexpr` gave it an interpreter. Concepts gave it a type system. Reflection gives it eyes.

Each addition made the compile-time language more capable, more ergonomic, more like a real programming language. The trajectory is clear: C++ is converging toward a world where the compile-time language and the runtime language are the same language, just evaluated at different times.

We're not there yet. The syntax is still different (`^^` and `[::]` are not pretty). The error messages still need work. The ecosystem is just beginning to build on these capabilities. But the foundation is solid, and the direction is unmistakable.

The compile-time language inside C++ started as an accident — Turing-completeness emerging unbidden from template instantiation rules. It's ending as a design. A deliberate, powerful, metaprogramming system built into the world's most widely deployed systems programming language. And now it can see.
