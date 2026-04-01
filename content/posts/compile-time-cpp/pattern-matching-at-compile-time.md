---
title: "Pattern Matching at Compile Time — Template Specialization as Structural Decomposition"
date: 2026-02-15
slug: pattern-matching-at-compile-time
tags: compile-time-cpp, specialization, pattern-matching, partial-specialization
excerpt: Template specialization isn't just an override mechanism. It's the compile-time language's pattern matching — it takes types apart, examines their bones, and lets you react to their shape.
---

If you've used Rust, Haskell, or ML, you know the joy of pattern matching. You take a value, you describe the shapes it could have, and the language destructures it and hands you the pieces. `match x { Some(inner) => ..., None => ... }`. Clean. Powerful. Expressive.

C++ has pattern matching too. It's been there since 1998. It just doesn't look like pattern matching, so nobody calls it that.

Template specialization is pattern matching on types. Not metaphorically. *Literally.* You write a primary template (the default case), then write specializations that match specific structural patterns. The compiler tries to match your argument against each pattern, picks the best one, and — here's the good part — *takes the type apart*, binding its structural components to template parameters that you can use.

Previous posts introduced specialization as compile-time branching. That framing is correct but it undersells what's happening. Specialization doesn't just select between alternatives. It *decomposes*. It takes a compound type apart and hands you the pieces. `T*` isn't just "a pointer" — it's a pattern that binds `T` to whatever's underneath the pointer. The compiler does the destructuring automatically.

Once you see specialization as pattern matching, the entire `<type_traits>` header becomes a catalog of pattern-matching functions. And you can write your own.

## Full Specialization: The Exact Match

The simplest form of pattern matching: match an exact type. No decomposition, no binding — just "if the type is exactly this, do that."

```cpp
template<typename T>
struct type_name {
    static constexpr auto value = "unknown";
};

template<>
struct type_name<int> {
    static constexpr auto value = "int";
};

template<>
struct type_name<double> {
    static constexpr auto value = "double";
};

template<>
struct type_name<std::string> {
    static constexpr auto value = "string";
};
```

`type_name<int>::value` is `"int"`. `type_name<float>::value` is `"unknown"`. It's a lookup table. Useful, but limited — you need a separate specialization for every type you care about. That doesn't scale.

## Partial Specialization: The X-Ray Machine

Partial specialization is where the real magic lives. Instead of matching a specific type, you match a *structural pattern*. The compiler deduces what fits where, binding the pieces to template parameters you can use.

```cpp
template<typename T>
struct is_pointer : std::false_type {};

template<typename T>
struct is_pointer<T*> : std::true_type {};
```

When you write `is_pointer<int*>`, the compiler sees the specialization `is_pointer<T*>` and asks: "Can I match `int*` against the pattern `T*`?" Yes — `T = int`. Specialization selected. For `is_pointer<double>`, nothing matches the `T*` pattern, so the primary template (the default "no") wins.

`T*` is a pattern. `T` is a binding variable. The compiler performs *unification* — the same algorithm that Hindley-Milner type inference uses — to check if the argument matches the pattern and what the bindings should be.

This is the same thing as `match value { Some(inner) => ... }` in Rust, except you're matching on types instead of values. The compiler takes `int*` apart, sees it's a pointer, and binds `T` to `int`. Structural pattern matching, performed by the compiler at instantiation time.

## Taking Types Apart

C++ has a rich set of type constructors — pointers, references, arrays, functions, member pointers, templates. Partial specialization can decompose all of them. It's like having X-ray vision for types.

**Pointers and references** — strip one layer:

```cpp
template<typename T> struct remove_pointer       { using type = T; };
template<typename T> struct remove_pointer<T*>    { using type = T; };

template<typename T> struct remove_reference      { using type = T; };
template<typename T> struct remove_reference<T&>  { using type = T; };
template<typename T> struct remove_reference<T&&> { using type = T; };
```

Three patterns: `T*`, `T&`, `T&&`. Each strips one layer and binds what's underneath. `remove_reference<int&&>::type` matches `T&&` with `T = int`, yielding `int`. The compiler did the destructuring for you.

**Arrays with their size** — extract both the element type AND the length:

```cpp
template<typename T>
struct array_traits {
    static constexpr bool is_array = false;
};

template<typename T, std::size_t N>
struct array_traits<T[N]> {
    static constexpr bool is_array = true;
    using element_type = T;
    static constexpr std::size_t size = N;
};
```

The pattern `T[N]` binds both the element type AND the array extent. `array_traits<int[5]>` deduces `T = int`, `N = 5`. One specialization, two pieces of information extracted. The compiler took `int[5]` apart and gave you both the `int` and the `5`.

**Function types** — this one's really cool:

```cpp
template<typename T>
struct function_traits;  // primary undefined — we only handle function types

template<typename R, typename... Args>
struct function_traits<R(Args...)> {
    using return_type = R;
    using args = std::tuple<Args...>;
    static constexpr std::size_t arity = sizeof...(Args);
};
```

`function_traits<int(double, char)>` matches the pattern `R(Args...)` with `R = int`, `Args = {double, char}`. You've decomposed a function type into its return type and parameter list. You now know the return type, the argument types, and how many arguments there are — all extracted from the type itself, at compile time.

This is the foundation of things like `std::function`'s internal machinery. When you write `std::function<int(double, char)>`, the library uses exactly this kind of pattern matching to figure out what the callable should look like.

**Pointer to member** — for the truly adventurous:

```cpp
template<typename T>
struct member_pointer_traits;

template<typename T, typename Class>
struct member_pointer_traits<T Class::*> {
    using member_type = T;
    using class_type = Class;
};
```

`member_pointer_traits<int Foo::*>` deduces `T = int`, `Class = Foo`. You've taken apart a pointer-to-member type. This shows up in serialization libraries, ORM systems, and anywhere you need to programmatically access struct members.

**Matching specific template instantiations:**

```cpp
template<typename T>
struct is_vector : std::false_type {};

template<typename T, typename Alloc>
struct is_vector<std::vector<T, Alloc>> : std::true_type {
    using element_type = T;
    using allocator_type = Alloc;
};
```

The pattern `std::vector<T, Alloc>` matches any `std::vector` and binds its template arguments. You can do this for any template — match the outer template and decompose its parameters. "Is this a `std::optional`? Great, what's inside it?"

## Matching on Values Too

Specialization isn't limited to types. Non-type template parameters can be matched too, which gives you pattern matching on compile-time integers, booleans, and (since C++20) structural types:

```cpp
template<int N>
struct factorial {
    static constexpr int value = N * factorial<N - 1>::value;
};

template<>
struct factorial<0> {
    static constexpr int value = 1;
};
```

The specialization `factorial<0>` matches the integer `0` exactly. Every other integer hits the primary template, which recurses. This is identical in structure to a recursive function in Haskell:

```
factorial 0 = 1
factorial n = n * factorial (n - 1)
```

Same logic. Different syntax. The C++ version has more angle brackets, but the *thinking* is identical.

## Nested Patterns: Going Deep

Patterns can nest. You can match `T**` (pointer to pointer), `const T*` (pointer to const), or even deeply nested template instantiations. The compiler will peel back as many layers as your pattern specifies.

```cpp
template<typename T>
struct strip_all_pointers {
    using type = T;  // base case: not a pointer
};

template<typename T>
struct strip_all_pointers<T*> {
    using type = typename strip_all_pointers<T>::type;  // recurse
};
```

`strip_all_pointers<int***>::type` matches `T*` with `T = int**`, recurses, matches again with `T = int*`, recurses again, matches with `T = int`, hits the primary template, returns `int`. Three layers of pointer, recursively peeled away by pattern matching.

You can match specific nested structures too:

```cpp
template<typename T>
struct is_vector_of_pairs : std::false_type {};

template<typename K, typename V, typename Alloc>
struct is_vector_of_pairs<std::vector<std::pair<K, V>, Alloc>> : std::true_type {};
```

This matches `std::vector<std::pair<K, V>, Alloc>` — a vector of pairs. It decomposes through *two layers* of template instantiation, binding `K`, `V`, and `Alloc` in one shot. The compiler looks at the argument, sees it's a vector, sees the element type is a pair, and gives you the key type, value type, and allocator. All at compile time.

## Ambiguity and Partial Ordering

When multiple specializations match, the compiler picks the most specific one. The rule is usually intuitive:

```cpp
template<typename T>           struct classify          { static constexpr int id = 0; };
template<typename T>           struct classify<T*>      { static constexpr int id = 1; };
template<typename T>           struct classify<const T> { static constexpr int id = 2; };
template<typename T>           struct classify<const T*>{ static constexpr int id = 3; };
```

`classify<const int*>` — both `T*` (with `T = const int`) and `const T` (with `T = int*`) match. But `const T*` *also* matches, with `T = int`. The compiler picks `const T*` because it's the most specific — it constrains both the pointer structure and the const qualifier.

The formal rule: specialization A is more specialized than B if every argument that matches A also matches B, but not vice versa. `const T*` is more specific than `T*` because every `const T*` is a `T*`, but not every `T*` is a `const T*`.

When the compiler can't find a unique most-specific match, it gives up and reports an ambiguity error:

```cpp
template<typename T, typename U> struct pair_classify {};
template<typename T>             struct pair_classify<T, T>  {};  // same types
template<typename T>             struct pair_classify<T, int> {};  // second is int

pair_classify<int, int> x;  // Error: ambiguous! Both match equally well.
```

Both `<T, T>` with `T = int` and `<T, int>` with `T = int` match, and neither is more specific than the other. This is the compile-time equivalent of a "which do you mean?" error.

## A Practical Example: Type Classifier

Let's combine everything into a compile-time type classifier that categorizes *any* type by its structural shape:

```cpp
template<typename T>
struct type_class {
    static constexpr auto value = "value";
};

template<typename T>
struct type_class<T*> {
    static constexpr auto value = "pointer";
};

template<typename T>
struct type_class<T&> {
    static constexpr auto value = "lvalue_reference";
};

template<typename T>
struct type_class<T&&> {
    static constexpr auto value = "rvalue_reference";
};

template<typename T, std::size_t N>
struct type_class<T[N]> {
    static constexpr auto value = "array";
};

template<typename R, typename... Args>
struct type_class<R(Args...)> {
    static constexpr auto value = "function";
};

template<typename T, typename Class>
struct type_class<T Class::*> {
    static constexpr auto value = "member_pointer";
};

static_assert(type_class<int>::value == std::string_view("value"));
static_assert(type_class<int*>::value == std::string_view("pointer"));
static_assert(type_class<int&>::value == std::string_view("lvalue_reference"));
static_assert(type_class<int&&>::value == std::string_view("rvalue_reference"));
static_assert(type_class<int[5]>::value == std::string_view("array"));
static_assert(type_class<void(int)>::value == std::string_view("function"));
```

Seven patterns. Each matches a different structural shape. The compiler does all the classification at compile time. No runtime cost, no function calls, no branches. Pure structural decomposition.

This is a toy example, but the technique scales. The real `std::is_function` in major standard library implementations has *dozens* of specializations covering every combination of cv-qualifier, ref-qualifier, `noexcept`, and C variadic. Each one is a pattern. The primary template covers everything that isn't a function. The specializations enumerate every shape a function type can take.

## The Mental Model

Template specialization is often taught as "providing a different implementation for specific types." That framing makes full specialization obvious and partial specialization mysterious. Here's the better framing:

**Specialization is pattern matching.** The primary template is the catch-all default. Each specialization is a structural pattern. The compiler matches the argument against each pattern, finds the best one, takes the argument apart, and binds the pieces to template parameters.

Once you internalize this, the entire machinery of `<type_traits>` becomes readable:

- `remove_pointer` is a one-pattern match that strips `T*` to `T`
- `remove_reference` is a two-pattern match over `T&` and `T&&`
- `is_function` is a many-pattern match over every possible function shape
- `decay` is a chain of pattern matches: array to pointer, function to function pointer, then strip cv-qualifiers

The compile-time language's pattern matching is more verbose than Rust's `match` or Haskell's case expressions. You need more angle brackets and colons. But the expressive power is the same. You can match on structure at any depth, bind components, and recurse. That's all a pattern matching system needs.

Now go look at `<type_traits>` again. I promise it'll make a lot more sense.
