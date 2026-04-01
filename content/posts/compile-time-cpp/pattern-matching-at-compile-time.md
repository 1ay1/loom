---
title: "Pattern Matching at Compile Time — Template Specialization as Structural Decomposition"
date: 2026-02-15
slug: pattern-matching-at-compile-time
tags: compile-time-cpp, specialization, pattern-matching, partial-specialization
excerpt: Template specialization isn't just an override mechanism. It's the compile-time language's pattern matching — decomposing types by their structure.
---

Earlier posts introduced template specialization as compile-time branching. That framing is correct but undersells what's actually happening. Specialization doesn't just select between alternatives. It *decomposes*. It takes a compound type apart, binding its structural components to template parameters, then hands you those components to work with. This is pattern matching in exactly the same sense that Haskell, ML, and Rust use the term.

In those languages, you pattern match on values. You write `match x { Some(inner) => ..., None => ... }` and the language destructures the value, binding `inner` to whatever's inside the `Some`. In compile-time C++, you pattern match on types. You write a partial specialization that matches `T*`, and the compiler destructures the pointer type, binding `T` to the pointed-to type. The mechanism is syntactically different. The semantics are the same.

The primary template is the default case — the wildcard pattern, the `_ =>` arm. Specializations are the specific patterns. The compiler tries to match the argument against every specialization, picks the best one, and falls back to the primary template if nothing matches. This is the entire control flow model of the compile-time language, and understanding it deeply is the key to reading (and writing) serious template metaprogramming.

## Full Specialization: Exact Match

The simplest form of pattern matching is matching an exact type. No decomposition, no binding — just "if the type is exactly this, do that."

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
```

`type_name<int>::value` is `"int"`. `type_name<float>::value` is `"unknown"`. This is the equivalent of matching against literal values in a `match` expression. It's useful but limited — you need a separate specialization for every type you care about.

## Partial Specialization: Structural Decomposition

Partial specialization is where the real power lives. Instead of matching a specific type, you match a *pattern* — a structural shape that the type must conform to. The compiler deduces the template parameters from the structure of the argument, binding them for use inside the specialization.

```cpp
template<typename T>
struct is_pointer : std::false_type {};

template<typename T>
struct is_pointer<T*> : std::true_type {};
```

When you write `is_pointer<int*>`, the compiler sees the specialization `is_pointer<T*>` and asks: can I match `int*` against the pattern `T*`? Yes — `T = int`. So it picks the specialization. For `is_pointer<double>`, no specialization matches, so it falls back to the primary template. `false_type`.

The specialization `T*` is a pattern. The `T` is a binding variable. The compiler performs unification — the same algorithm that Hindley-Milner type inference uses — to determine whether the argument matches the pattern and what the bindings should be. This is structural pattern matching on types, performed by the compiler at instantiation time.

## Decomposing Compound Types

C++ has a rich algebra of type constructors. Pointers, references, arrays, functions, member pointers, templates. Partial specialization can match against all of them, decomposing each into its constituent parts.

**Pointers and references:**

```cpp
template<typename T> struct remove_pointer       { using type = T; };
template<typename T> struct remove_pointer<T*>    { using type = T; };

template<typename T> struct remove_reference      { using type = T; };
template<typename T> struct remove_reference<T&>  { using type = T; };
template<typename T> struct remove_reference<T&&> { using type = T; };
```

Three patterns: `T*`, `T&`, `T&&`. Each strips one layer of type constructor and binds what's underneath. `remove_reference<int&&>::type` matches `T&&` with `T = int`, yielding `int`.

**Arrays with size:**

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

The pattern `T[N]` binds both the element type and the array extent. `array_traits<int[5]>` deduces `T = int`, `N = 5`. A single specialization extracts two pieces of information from the type's structure.

**Function types:**

```cpp
template<typename T>
struct function_traits;  // primary undefined — we only handle functions

template<typename R, typename... Args>
struct function_traits<R(Args...)> {
    using return_type = R;
    using args = std::tuple<Args...>;
    static constexpr std::size_t arity = sizeof...(Args);
};
```

`function_traits<int(double, char)>` matches the pattern `R(Args...)` with `R = int`, `Args = {double, char}`. You've decomposed a function type into its return type and parameter list. This is one of the most practically useful specialization patterns — it's the foundation of things like `std::function`'s internal machinery and reflection libraries.

**Pointer to member:**

```cpp
template<typename T>
struct member_pointer_traits;

template<typename T, typename Class>
struct member_pointer_traits<T Class::*> {
    using member_type = T;
    using class_type = Class;
};
```

`member_pointer_traits<int Foo::*>` deduces `T = int`, `Class = Foo`. You've taken apart a pointer-to-member type into the member's type and the class it belongs to.

**Template template matching:**

```cpp
template<typename T>
struct is_vector : std::false_type {};

template<typename T, typename Alloc>
struct is_vector<std::vector<T, Alloc>> : std::true_type {
    using element_type = T;
    using allocator_type = Alloc;
};
```

The pattern `std::vector<T, Alloc>` matches any `std::vector` instantiation and binds its template arguments. This works for any template — you're matching the outer template and decomposing its parameters.

## Matching on Values

Specialization isn't limited to types. Non-type template parameters can be matched too, giving you pattern matching on compile-time integers, booleans, and (since C++20) structural types.

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

The full specialization `factorial<0>` matches the integer `0` exactly. Every other integer hits the primary template, which recurses. This is the same structure as a recursive function definition with a base case in Haskell:

```
factorial 0 = 1
factorial n = n * factorial (n - 1)
```

Identical logic. Different syntax.

## Nested Patterns

Patterns can nest. You can match `T**` (pointer to pointer), `const T*` (pointer to const), or even deeply nested template instantiations.

```cpp
template<typename T>
struct strip_all_pointers {
    using type = T;
};

template<typename T>
struct strip_all_pointers<T*> {
    using type = typename strip_all_pointers<T>::type;
};
```

`strip_all_pointers<int***>::type` matches `T*` with `T = int**`, recurses, matches again with `T = int*`, recurses, matches again with `T = int`, hits the primary template, returns `int`. Recursive structural decomposition, driven entirely by pattern matching.

You can also match specific nested structures:

```cpp
template<typename T>
struct is_pair : std::false_type {};

template<typename K, typename V>
struct is_pair<std::pair<K, V>> : std::true_type {};

template<typename T>
struct is_vector_of_pairs : std::false_type {};

template<typename K, typename V, typename Alloc>
struct is_vector_of_pairs<std::vector<std::pair<K, V>, Alloc>> : std::true_type {};
```

The second specialization matches `std::vector<std::pair<K, V>, Alloc>` — a vector of pairs. It decomposes through two layers of template instantiation, binding `K`, `V`, and `Alloc` in one shot.

## Ambiguity and Partial Ordering

When multiple specializations match an argument, the compiler must decide which one is "more specialized." The rule is intuitive in simple cases and subtle in complex ones.

```cpp
template<typename T>           struct classify         { static constexpr int id = 0; };
template<typename T>           struct classify<T*>     { static constexpr int id = 1; };
template<typename T>           struct classify<const T>{ static constexpr int id = 2; };
template<typename T>           struct classify<const T*>{ static constexpr int id = 3; };
```

`classify<int>` — only the primary matches. `id = 0`.

`classify<int*>` — `T*` matches with `T = int`. `id = 1`.

`classify<const int>` — `const T` matches with `T = int`. `id = 2`.

`classify<const int*>` — both `T*` (with `T = const int`) and `const T` (with `T = int*`) match. But `const T*` also matches, with `T = int`. The compiler picks `const T*` because it's more specialized — it constrains both the pointer structure and the const qualifier, while the others only constrain one. `id = 3`.

The formal rule: specialization A is more specialized than B if every argument that matches A also matches B, but not vice versa. `const T*` is more specialized than `T*` because every `const T*` is a `T*`, but not every `T*` is a `const T*`.

When the compiler can't determine a unique most-specialized match, it's an ambiguity error:

```cpp
template<typename T, typename U> struct pair_classify {};
template<typename T>             struct pair_classify<T, T>  {};  // same types
template<typename T>             struct pair_classify<T, int> {};  // second is int

pair_classify<int, int> x;  // Error: ambiguous. Both specializations match.
```

Both `<T, T>` with `T = int` and `<T, int>` with `T = int` match. Neither is more specialized than the other. The compiler rejects the code.

## Combining Specialization with Inheritance

The `std::true_type` / `std::false_type` idiom turns specialization into a type predicate system. By inheriting from these base classes, your traits carry not just a `value` member but an entire interface: `value_type`, implicit conversion to `bool`, `operator()`, and compatibility with tag dispatch.

Complex predicates compose from simpler ones:

```cpp
template<typename T>
struct is_string :
    std::bool_constant<
        std::is_same_v<T, std::string> ||
        std::is_same_v<T, std::string_view> ||
        std::is_same_v<std::decay_t<T>, const char*>
    > {};
```

But the most powerful compositions use specialization layers. Here's a simplified `is_function` — one of the most specialization-heavy traits in `<type_traits>`:

```cpp
template<typename T>
struct is_function : std::false_type {};

// Regular function
template<typename R, typename... Args>
struct is_function<R(Args...)> : std::true_type {};

// Function with const qualifier (member function type)
template<typename R, typename... Args>
struct is_function<R(Args...) const> : std::true_type {};

// Variadic function (C-style)
template<typename R, typename... Args>
struct is_function<R(Args..., ...)> : std::true_type {};

// And many more: volatile, &, &&, noexcept, and combinations thereof...
```

The real `std::is_function` in major standard library implementations has dozens of specializations covering every combination of cv-qualifier, ref-qualifier, `noexcept`, and C variadic. Each is a distinct pattern that the compiler matches against. The primary template covers everything that isn't a function type. The specializations enumerate every structural shape a function type can take.

## Practical Example: A Type Classifier

Let's combine everything into a compile-time type classifier — a metafunction that categorizes any type into a descriptive string.

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

Seven patterns. Each one matches a different structural shape. The compiler does all the work — no runtime cost, no function calls, no branches. Just pure structural decomposition at compile time.

## The Mental Model

Template specialization is often taught as "providing a different implementation for specific types." That framing makes full specialization obvious and partial specialization mysterious. The better framing: specialization is pattern matching. The primary template is the catch-all case. Each specialization is a pattern that the compiler matches against, structurally decomposing the argument and binding its components.

Once you internalize this, the entire machinery of `<type_traits>` becomes legible. `remove_pointer` is a one-pattern match that strips `T*` to `T`. `remove_reference` is a two-pattern match over `T&` and `T&&`. `is_function` is a many-pattern match over every possible function type shape. `decay` is a chain of pattern matches: array to pointer, function to function pointer, then strip cv-qualifiers.

The compile-time language's pattern matching is more verbose than Rust's `match` or Haskell's case expressions. But the expressive power is the same. You can match on structure at any depth, bind components, and recurse. That's all you need.
