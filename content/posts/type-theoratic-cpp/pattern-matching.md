---
title: "Pattern Matching — Deconstructing Values by Shape"
date: 2026-03-22
slug: pattern-matching
tags: [c++20, type-theory, pattern-matching, elimination-rules, exhaustiveness, case-analysis]
excerpt: "Pattern matching is the elimination form for algebraic types — the way you take apart what introduction rules put together. Exhaustiveness, nested patterns, guards, refutability, and the deep reason why the compiler insists you handle every case."
---

In the [previous post](/post/algebraic-data-types), we built types using two operations: products (structs, tuples) and sums (variants). We learned that `std::visit` eliminates a variant by providing one handler per alternative. We saw the `overloaded` trick. We even computed areas of shapes.

But we skipped a question. *Why* does `std::visit` require one handler per alternative? Why does the compiler reject your program if you forget a case? And what happens when your data is nested — a variant inside a variant, a struct inside a variant, a list of variants?

The answer is **pattern matching** — the systematic deconstruction of values according to their type structure. Pattern matching is not a convenience feature. It is the **elimination rule** for algebraic types, and elimination rules are half of what makes a type meaningful. Without them, you can put data into a type but you can never get it out.

This post gives you the full theory behind pattern matching: what it is, why it must be exhaustive, how it composes when patterns nest, and how C++ approximates a feature that languages like ML, Haskell, and Rust provide natively. By the end, you will understand why a missing case is not a warning — it is a logical gap.

## What Is a Pattern?

Start with the simplest example. You have a boolean and you want to act on it:

```cpp
if (b) {
    do_true_thing();
} else {
    do_false_thing();
}
```

This is pattern matching on `bool`. A `bool` is the sum type `1 + 1` — two alternatives, each carrying no data. The `if/else` provides one handler for each alternative. Every value of type `bool` is matched by exactly one branch.

Now generalize. Suppose you have a sum type with three alternatives, each carrying different data:

```cpp
struct Circle    { double radius; };
struct Rectangle { double width, height; };
struct Triangle  { double base, height; };

using Shape = std::variant<Circle, Rectangle, Triangle>;
```

Pattern matching on `Shape` means: inspect which alternative is present, extract the data it carries, and execute the corresponding code. In C++:

```cpp
auto describe(const Shape& s) -> std::string {
    return std::visit(overloaded{
        [](const Circle& c)    { return "circle r=" + std::to_string(c.radius); },
        [](const Rectangle& r) { return "rect " + std::to_string(r.width) + "x" + std::to_string(r.height); },
        [](const Triangle& t)  { return "tri b=" + std::to_string(t.base); },
    }, s);
}
```

Each lambda is a **pattern clause**: a pattern (the parameter type) paired with a body (the code to execute). The variant dispatches to the clause whose pattern matches the active alternative.

This is the core idea. A pattern is a description of the *shape* of a value. Matching a value against a pattern does two things simultaneously: it **tests** whether the value has that shape, and if so, it **binds** the components of the value to names so you can use them.

## Patterns in Type Theory: The Elimination Rule

In [part 1](/post/type-theoretic-foundations), we established that every type has three components:

- **Formation rule**: how to declare the type
- **Introduction rules**: how to create values of the type
- **Elimination rules**: how to use values of the type

Pattern matching *is* the elimination rule. For sum types, the elimination rule says:

> To use a value of type `A + B`, you must provide a function from `A` to some result type `C`, and a function from `B` to `C`. The result is a value of type `C`.

In formal notation:

```
Γ ⊢ e : A + B    Γ, x:A ⊢ e₁ : C    Γ, y:B ⊢ e₂ : C
─────────────────────────────────────────────────────────  (+E)
                    Γ ⊢ case e of { inl x → e₁ ; inr y → e₂ } : C
```

Read this as: if `e` has type `A + B`, and you can produce a `C` from an `A`, and you can produce a `C` from a `B`, then the case expression produces a `C`. The `x` and `y` are the *bindings* — the names that pattern matching gives you for the inner data.

The introduction rules for `A + B` are:
- `inl(a)` — inject a value of type `A` into the left alternative
- `inr(b)` — inject a value of type `B` into the right alternative

The elimination rule is the *inverse* of introduction. Introduction puts data in. Elimination takes data out. They are duals, and together they define the type completely.

For product types, the elimination rule is simpler:

```
Γ ⊢ e : A × B
──────────────  (×E₁)        ──────────────  (×E₂)
Γ ⊢ π₁(e) : A               Γ ⊢ π₂(e) : B
```

You eliminate a product by projecting — accessing the first component or the second. In C++, this is `pair.first`, `std::get<0>(tuple)`, or `struct.field`. No case analysis needed because a product always has all its fields.

The asymmetry is important: **sums require case analysis (pattern matching), products require projection**. This is because a sum has *alternatives* — you must determine which one is present. A product has no alternatives — all fields are always there.

## Why Exhaustiveness Is Non-Negotiable

Consider this broken code:

```cpp
auto area(const Shape& s) -> double {
    return std::visit(overloaded{
        [](const Circle& c)    { return 3.14159 * c.radius * c.radius; },
        [](const Rectangle& r) { return r.width * r.height; },
        // forgot Triangle!
    }, s);
}
```

This does not compile. `std::visit` requires that the visitor handle every alternative of the variant. But why is the compiler so strict? Why not just produce a runtime error for the missing case?

The answer comes from the elimination rule. Look at it again:

```
Γ ⊢ e : A + B    Γ, x:A ⊢ e₁ : C    Γ, y:B ⊢ e₂ : C
─────────────────────────────────────────────────────────
              Γ ⊢ case e of { inl x → e₁ ; inr y → e₂ } : C
```

The rule has THREE premises. All three must be satisfied for the conclusion to hold. If you omit the `B → C` case, you cannot derive the conclusion. The proof is incomplete. The type `C` is not produced. The expression has no type.

This is not a design choice — it is a logical necessity. A function `A + B → C` that only handles `A` is not a function from `A + B`. It is a **partial** function, and partial functions are lies in the type system. They claim to accept any `A + B` but fail on some inputs. The type says "I handle all shapes" but the code says "I only handle two."

Exhaustiveness checking is the compiler verifying that your elimination is total. It is not a convenience — it is soundness.

### What Happens Without Exhaustiveness

Languages that lack exhaustive pattern matching pay in runtime crashes. In C, a `switch` on an enum has no exhaustiveness requirement:

```c
enum Shape { CIRCLE, RECTANGLE, TRIANGLE };

double area(enum Shape kind, double a, double b) {
    switch (kind) {
        case CIRCLE:    return 3.14159 * a * a;
        case RECTANGLE: return a * b;
        // no TRIANGLE case
    }
    // falls off the end — undefined behavior
}
```

The C compiler might warn, but it compiles. At runtime, calling `area(TRIANGLE, 3.0, 4.0)` is undefined behavior. The function promised to return a `double` but does not. The promise was a lie.

In a language with proper sum types and exhaustive matching — ML, Haskell, Rust, or C++ with `std::visit` — this cannot happen. The type system and the pattern matcher conspire to guarantee that every possible input is handled.

## Case Analysis: The Logical Perspective

Pattern matching has a logical name: **case analysis** (also called **proof by cases** or **disjunction elimination** in natural deduction).

The logical rule is:

```
  A ∨ B      A ⊢ C      B ⊢ C
  ─────────────────────────────  (∨E)
              C
```

"If you know `A or B`, and you can derive `C` from `A`, and you can derive `C` from `B`, then you can derive `C`."

Under the Curry-Howard correspondence from [part 6](/post/concepts-as-logic), this is *exactly* pattern matching:

| Logic | Types | C++ |
|-------|-------|-----|
| `A ∨ B` | `variant<A, B>` | the value being matched |
| `A ⊢ C` | `A → C` | the first lambda in `overloaded{...}` |
| `B ⊢ C` | `B → C` | the second lambda |
| `C` | return type | what `std::visit` returns |

Every `std::visit` is a proof by cases. You have a disjunction (the variant). You prove a conclusion (the return type) by showing it follows from each disjunct separately.

## Patterns Compose: Nested Matching

So far, our patterns have been flat: match on the top-level variant, extract one layer of data. But real data is nested. A variant might contain a struct that contains another variant. Patterns need to reach inside.

### Nested Patterns in Theory

In languages with built-in pattern matching, patterns compose naturally:

```haskell
-- Haskell: nested pattern on a pair inside a sum
describe :: Either (String, Int) Bool -> String
describe (Left (name, age)) = name ++ " is " ++ show age
describe (Right True)       = "yes"
describe (Right False)      = "no"
```

Each pattern descends into the structure: `Left (name, age)` first matches the `Left` constructor (sum elimination), then destructures the pair inside it (product elimination). The depth is unlimited — you can match arbitrarily deep nesting in a single clause.

### Nested Matching in C++

C++ does not have built-in nested pattern syntax. You compose matches manually:

```cpp
using Payload = std::variant<std::pair<std::string, int>, bool>;

auto describe(const Payload& p) -> std::string {
    return std::visit(overloaded{
        [](const std::pair<std::string, int>& pair) {
            return pair.first + " is " + std::to_string(pair.second);
        },
        [](bool b) {
            return b ? std::string("yes") : std::string("no");
        },
    }, p);
}
```

The first level of matching happens in `std::visit`. The second level — deconstructing the pair — happens via member access (`pair.first`, `pair.second`). It works, but the two levels use different syntax and different mechanisms.

For deeper nesting, you stack visits:

```cpp
using Inner = std::variant<int, std::string>;
using Outer = std::variant<Inner, double>;

auto process(const Outer& o) -> std::string {
    return std::visit(overloaded{
        [](const Inner& inner) {
            return std::visit(overloaded{
                [](int i)                { return std::to_string(i); },
                [](const std::string& s) { return s; },
            }, inner);
        },
        [](double d) { return std::to_string(d); },
    }, o);
}
```

Two nested `std::visit` calls, one for each sum-type layer. The structure mirrors the type nesting. This is correct but verbose — in ML or Rust, the same logic would be a flat list of nested patterns.

### Structured Bindings: Product Elimination

C++17 structured bindings provide pattern matching for products:

```cpp
auto [x, y, z] = some_tuple;
auto [name, age] = some_pair;
auto& [key, value] = *map_iterator;
```

This IS product elimination — the `π₁` and `π₂` projections from type theory, applied to all fields at once. It binds names to the components of a product without explicitly naming projections.

Combined with `std::visit`, you get both sum elimination (the visit) and product elimination (the binding):

```cpp
auto handle(const std::variant<std::pair<int,int>, std::string>& v) {
    return std::visit(overloaded{
        [](const std::pair<int,int>& p) {
            auto [a, b] = p;
            return a + b;
        },
        [](const std::string& s) {
            return static_cast<int>(s.size());
        },
    }, v);
}
```

## Refutable and Irrefutable Patterns

Not all patterns are created equal. There is a fundamental distinction between patterns that can fail and patterns that always succeed.

An **irrefutable pattern** always matches. It cannot fail. Product patterns are irrefutable — a tuple always has all its components, a struct always has all its fields. When you write `auto [x, y] = pair`, the match always succeeds. There is nothing to check.

A **refutable pattern** may fail. Sum patterns are refutable — a variant may or may not hold a particular alternative. When `std::visit` dispatches to the `Circle` handler, it first checks whether the variant holds a `Circle`. If it does not, the pattern fails and the next clause is tried.

This distinction matters because:

- **Irrefutable patterns** can appear in variable bindings (`auto [a, b] = ...`) and function parameters. They never need fallback logic.
- **Refutable patterns** must appear in contexts that handle failure — `if` conditions, match arms with alternatives, loops. They MUST be part of an exhaustive set.

In the elimination rules:

```
Product elimination: Γ ⊢ e : A × B
                     ───────────────   always succeeds — irrefutable
                     Γ ⊢ π₁(e) : A

Sum elimination:     Γ ⊢ e : A + B     requires BOTH branches — refutable individually,
                     ───────────────    exhaustive collectively
                     must handle A and B
```

A product has no alternatives. A sum does. That is the root of the distinction.

### `if constexpr` as Compile-Time Pattern Matching

C++ offers a compile-time variant of refutable matching:

```cpp
template<typename T>
auto serialize(const T& value) -> std::string {
    if constexpr (std::is_integral_v<T>) {
        return std::to_string(value);
    } else if constexpr (std::is_floating_point_v<T>) {
        return std::to_string(value);
    } else if constexpr (std::is_same_v<T, std::string>) {
        return "\"" + value + "\"";
    } else {
        static_assert(always_false<T>, "unsupported type");
    }
}
```

This is pattern matching on *types* rather than values. Each `if constexpr` branch tests a type predicate. Only the matching branch is compiled. The `static_assert` in the `else` serves as the exhaustiveness check — it catches types that do not match any pattern.

But notice: this exhaustiveness is manual. The compiler does not force you to add the `static_assert`. If you omit it, an unmatched type silently gets no code, and the function might not return a value. `std::visit` is better here because it makes exhaustiveness automatic.

## Guards: When Types Are Not Enough

Sometimes the match depends not just on the shape of the data but on a *condition* involving the data's values. A guard is a boolean condition attached to a pattern clause:

```haskell
-- Haskell: patterns with guards
classify :: Int -> String
classify n
    | n < 0     = "negative"
    | n == 0    = "zero"
    | n < 100   = "small"
    | otherwise = "large"
```

Each guard refines the match. The pattern `n` is irrefutable (it always matches an `Int`), but the guards add conditions. If a guard fails, the next clause is tried.

C++ approximates guards with `if` statements inside pattern clauses:

```cpp
auto classify(int n) -> std::string {
    if (n < 0)       return "negative";
    if (n == 0)      return "zero";
    if (n < 100)     return "small";
    return "large";
}
```

Or, more structurally, inside a `std::visit`:

```cpp
auto describe_result(const Result& r) -> std::string {
    return std::visit(overloaded{
        [](int value) -> std::string {
            if (value == 0) return "zero result";
            if (value < 0)  return "negative result: " + std::to_string(value);
            return "positive result: " + std::to_string(value);
        },
        [](const Error& e) -> std::string {
            return "error: " + e.message;
        },
    }, r);
}
```

The outer dispatch (sum elimination) is exhaustive and compiler-checked. The inner guards (value conditions) are not — they rely on the programmer covering all cases. This is the weakness of combining structural matching with ad-hoc guards: the compiler's exhaustiveness checking stops at the structural level.

## Wildcard and Default Patterns

A **wildcard pattern** matches anything. It is the universal irrefutable pattern:

```haskell
-- Haskell: wildcard
f :: Either Int String -> Int
f (Left n)  = n
f (Right _) = 0   -- underscore = wildcard, ignores the string
```

In C++, a wildcard in `std::visit` is a generic lambda:

```cpp
std::visit(overloaded{
    [](int n)     { return n; },
    [](auto&&)    { return 0; },   // matches anything else
}, v);
```

The `auto&&` lambda accepts any type — it is the catch-all. But beware: **wildcards weaken exhaustiveness**. A catch-all clause silences the compiler. If you add a new alternative to the variant, the wildcard absorbs it silently. You get no compilation error, and potentially wrong behavior.

This is the fundamental trade-off:
- **Explicit patterns** for every alternative: verbose but safe. Adding an alternative produces a compile error everywhere it is unhandled.
- **Wildcard/default patterns**: concise but fragile. Adding an alternative is silently absorbed by the default.

The type-theoretic perspective: a wildcard is a valid proof, but it is a *non-informative* proof. It says "I can handle B by ignoring it." Logically sound, but it throws away information. Prefer explicit patterns unless you genuinely do not care about the alternative.

## Overlapping Patterns and Specificity

When multiple patterns could match, order matters. Languages handle this differently:

In ML and Haskell, patterns are tried **top to bottom**. The first matching pattern wins. Overlapping patterns are allowed but the compiler warns about unreachable clauses.

In C++ with `std::visit`, overlapping is resolved by **overload resolution**. The most specific match wins:

```cpp
std::visit(overloaded{
    [](int n)            { /* specific: matches int */ },
    [](const auto& x)    { /* generic: matches anything */ },
}, v);
```

If the variant holds an `int`, the first lambda is more specific and wins. If it holds anything else, the second lambda catches it. This is not pattern matching order — it is C++ overload resolution rules, which happen to produce the right behavior.

The type-theoretic concept here is **specificity ordering**. Given patterns P₁ and P₂, P₁ is more specific than P₂ if every value matching P₁ also matches P₂ but not vice versa. The most specific pattern should be preferred. In natural deduction, this is analogous to choosing the most informative proof when multiple proofs are available.

## Pattern Matching on Recursive Types

Pattern matching becomes truly powerful on recursive data. Consider the expression tree from [part 2](/post/algebraic-data-types):

```cpp
struct Literal { double value; };
struct Negate;
struct Add;

using Expr = std::variant<
    Literal,
    std::unique_ptr<Negate>,
    std::unique_ptr<Add>
>;

struct Negate { Expr operand; };
struct Add { Expr left, right; };
```

Evaluating this tree requires **recursive pattern matching** — match on the outer structure, then recursively match on the sub-expressions:

```cpp
auto eval(const Expr& e) -> double {
    return std::visit(overloaded{
        [](const Literal& lit) -> double {
            return lit.value;
        },
        [](const std::unique_ptr<Negate>& n) -> double {
            return -eval(n->operand);   // recurse into sub-expression
        },
        [](const std::unique_ptr<Add>& a) -> double {
            return eval(a->left) + eval(a->right);  // recurse into both
        },
    }, e);
}
```

Each recursive call is another round of pattern matching. The recursion terminates because the data structure is finite — the `Literal` base case carries no sub-expressions. This is the **structural recursion** principle: recursion guided by the structure of the data, where each recursive call operates on a structurally smaller sub-term.

Structural recursion is safe — it terminates by construction. The recursive type is the least fixed point `μX. F(X)`, meaning it is well-founded: every value is built up from a finite number of applications of the constructors. The catamorphism (fold) follows this structure exactly, peeling off one constructor at a time.

### Pattern Matching Compiles Expressions

The power of pattern matching on trees is not limited to evaluation. You can write simplifiers, optimizers, and transformers — all by matching on shape:

```cpp
// Simplify: -(-x) → x, 0 + x → x, x + 0 → x
auto simplify(Expr e) -> Expr {
    return std::visit(overloaded{
        [](Literal lit) -> Expr { return lit; },
        [&](std::unique_ptr<Negate> n) -> Expr {
            n->operand = simplify(std::move(n->operand));
            // Match: -(-x) → x
            if (auto* inner = std::get_if<std::unique_ptr<Negate>>(&n->operand)) {
                return simplify(std::move((*inner)->operand));
            }
            return std::move(n);
        },
        [&](std::unique_ptr<Add> a) -> Expr {
            a->left = simplify(std::move(a->left));
            a->right = simplify(std::move(a->right));
            // Match: 0 + x → x
            if (auto* l = std::get_if<Literal>(&a->left); l && l->value == 0.0) {
                return std::move(a->right);
            }
            // Match: x + 0 → x
            if (auto* r = std::get_if<Literal>(&a->right); r && r->value == 0.0) {
                return std::move(a->left);
            }
            return std::move(a);
        },
    }, std::move(e));
}
```

This is nested pattern matching: the outer `std::visit` matches the top-level constructor, and the inner `std::get_if` tests the sub-expressions. In a language with deep patterns:

```haskell
simplify (Negate (Negate x)) = simplify x
simplify (Add (Literal 0) x) = simplify x
simplify (Add x (Literal 0)) = simplify x
simplify other               = other
```

Four lines. Each line is a nested pattern that reaches two levels deep into the tree. The C++ version does the same work, but the nesting is expressed through control flow rather than pattern syntax.

## The Algebra of Patterns

Patterns form their own algebra — they can be combined, nested, and transformed according to rules.

### Pattern constructors

Every introduction rule for a type has a corresponding pattern:

| Type constructor | Introduction | Pattern |
|-----------------|-------------|---------|
| Product (A × B) | `make_pair(a, b)` | `auto [a, b] = ...` |
| Sum (A + B) | `variant<A,B>(a)` | `[](const A& a) { ... }` in visit |
| Unit (1) | `monostate{}` | `[](monostate) { ... }` |
| Recursive (μX.F(X)) | `make_unique<Cons>(...)` | `[](const unique_ptr<Cons>& c) { ... }` |

The pattern for each type mirrors its constructor. This is not coincidence — the elimination rule is *derived* from the introduction rules. The constructors tell you how values are built; the patterns tell you how to take them apart. They are inverses.

### Pattern composition

If you have a pattern for `A` and a pattern for `B`, you automatically have:
- A pattern for `A × B` (match both components)
- A pattern for `A + B` (match either alternative)
- A pattern for `F(A)` where F is a recursive functor (match one layer, recurse)

Patterns compose because types compose. The algebra of patterns is the *dual* of the algebra of types.

### Pattern equivalences

Just as types have algebraic equivalences (`A × 1 ≅ A`, `A + 0 ≅ A`), patterns do too:

- Matching against `pair<A, monostate>` is the same as matching against `A` (the unit field carries no information)
- A match against `variant<A>` (one alternative) is the same as a direct match against `A` (no case analysis needed)
- Matching `variant<A, A>` is the same as matching `A` twice with a tag — the two branches have identical structure

These are the **pattern isomorphisms**. They let you simplify match expressions just as the type algebra lets you simplify type expressions.

## How Pattern Matching Compiles

The compiler transforms pattern matching into efficient code. Understanding the compilation gives you insight into why patterns are structured the way they are.

### Decision trees

A set of pattern clauses compiles to a **decision tree**. Each internal node tests one component of the input (which variant alternative? what value?). Each leaf node executes the body of a matching clause.

For the Shape example:

```
       ┌─ index(s) ─┐
       │             │
  [0:Circle]   [1:Rectangle]   [2:Triangle]
       │             │               │
   λ(c)→...     λ(r)→...       λ(t)→...
```

`std::visit` inspects the variant index (stored as a small integer) and dispatches to the correct handler. This is typically compiled as a jump table or a switch — O(1) dispatch.

### Nested decisions

For nested patterns, the decision tree deepens:

```
       ┌── index(outer) ──┐
       │                   │
   [0:Inner]           [1:double]
       │                   │
  ┌─ index(inner) ─┐    λ(d)→...
  │                 │
[0:int]       [1:string]
  │                 │
λ(i)→...      λ(s)→...
```

Two levels of dispatch for two levels of nesting. The total work is proportional to the depth of nesting, not the total number of alternatives.

### Redundancy and usefulness

A good pattern match compiler also checks:

- **Exhaustiveness**: are all possible inputs covered? (Mandatory for soundness.)
- **Usefulness**: is every clause reachable? An unreachable clause indicates dead code.
- **Redundancy**: are there patterns that can never match because an earlier pattern already caught everything?

`std::visit` gets exhaustiveness from the type system (the overloaded visitor must accept all alternatives). Redundancy checking is done by overload resolution — if two lambdas accept the same type, the compiler reports ambiguity.

## C++ Today and Tomorrow

C++ does not have native pattern matching syntax — yet. The current tools are:

| Mechanism | What it matches | Exhaustive? |
|-----------|----------------|-------------|
| `std::visit` + `overloaded` | variant alternatives | Yes |
| Structured bindings | tuple/struct fields | Yes (irrefutable) |
| `if constexpr` | type predicates | Manual (static_assert) |
| `std::get_if` | single variant alternative | No (returns nullptr on mismatch) |
| `switch` on enum | enum values | Warning only |
| `dynamic_cast` | class hierarchy | No |

The gap is clear: C++ has exhaustive matching for sum types (`std::visit`) and product types (structured bindings), but no unified syntax that combines both and supports nesting.

### `std::get_if`: The Unsafe Escape Hatch

`std::get_if` is a non-exhaustive pattern match — it tests one alternative and returns a pointer:

```cpp
if (auto* c = std::get_if<Circle>(&shape)) {
    // use c->radius
} else if (auto* r = std::get_if<Rectangle>(&shape)) {
    // use r->width, r->height
}
// Triangle silently unhandled
```

This is **refutable matching without exhaustiveness**. You can forget a case and the compiler says nothing. It is useful for quick checks ("is this a Circle?") but dangerous for complete case analysis.

The rule of thumb: use `std::visit` when you need to handle ALL cases. Use `std::get_if` only when you genuinely care about one specific alternative and have a sound default for everything else.

### What Pattern Matching Proposals Would Give Us

Several proposals (P1371, P2688, and others) aim to bring native pattern matching to C++. The aspiration:

```cpp
// Hypothetical future C++ pattern matching syntax
inspect (shape) {
    <Circle c>    => 3.14159 * c.radius * c.radius;
    <Rectangle r> => r.width * r.height;
    <Triangle t>  => 0.5 * t.base * t.height;
};
```

The key features these proposals target:
- **Unified syntax** for sum and product patterns
- **Deep nesting** without manual visitor stacking
- **Guards** as first-class pattern refinements
- **Compiler-enforced exhaustiveness** with clear error messages
- **Binding by structure**, not by explicit accessor calls

Until these land, `std::visit` + `overloaded` is the best approximation. It is verbose but correct — it gets the exhaustiveness guarantee right, which is the single most important property of pattern matching.

## Pattern Matching in Other Type Systems

Understanding how other languages handle pattern matching illuminates what C++ is approximating and where it falls short.

### ML (1973): Where It Began

ML introduced algebraic data types with pattern matching as a first-class language construct:

```sml
datatype shape = Circle of real
               | Rectangle of real * real
               | Triangle of real * real

fun area (Circle r)        = Math.pi * r * r
  | area (Rectangle(w,h))  = w * h
  | area (Triangle(b,h))   = 0.5 * b * h
```

Three clauses. Each names the constructor and binds the payload. Exhaustive by default — omit a case and the compiler rejects the program. This is the gold standard: the type definition and the pattern match are syntactic mirrors of each other.

### Rust: Exhaustive by Default, `match` as Expression

Rust requires exhaustive `match`:

```rust
fn area(s: &Shape) -> f64 {
    match s {
        Shape::Circle { radius: r }          => PI * r * r,
        Shape::Rectangle { width: w, height: h } => w * h,
        Shape::Triangle { base: b, height: h }   => 0.5 * b * h,
    }
}
```

Like ML, every `match` must cover all variants. Rust also supports nested patterns, guards (`if` conditions), and `@` bindings (bind a name and match simultaneously). The `match` is an expression — it returns a value, enforcing that all arms have compatible types.

### Haskell: Patterns Everywhere

In Haskell, patterns appear in function definitions, `case` expressions, `let` bindings, lambda parameters, and list comprehensions. Patterns are a pervasive mechanism, not a special construct:

```haskell
head :: [a] -> a
head (x:_) = x
head []    = error "empty"

map :: (a -> b) -> [a] -> [b]
map _ []     = []
map f (x:xs) = f x : map f xs
```

The `(x:xs)` pattern simultaneously matches the list constructor `(:)`, binds the head to `x`, and binds the tail to `xs`. It is a nested pattern: a sum match (non-empty case) with a product match (head and tail) in one expression.

### The Common Thread

Every language with algebraic types provides pattern matching as the elimination form. The syntax varies, but the structure is identical:

1. A value of a sum type is inspected
2. Each alternative has a clause with bindings
3. All alternatives must be covered
4. The bodies all produce the same result type

This universality is not coincidence — it follows from the type theory. The elimination rule for sum types IS pattern matching. Any language with sum types must provide it, in some form.

## In Loom

Loom's codebase uses pattern matching — via `std::visit` and structural decomposition — at every level of the pipeline.

### Route Dispatch: Matching on URL Structure

The router matches incoming requests against route patterns, which is itself a form of pattern matching — not on algebraic types, but on structured data:

```cpp
std::visit(overloaded{
    [&](const StaticRoute& route) {
        // Exact path match — no parameters to extract
        serve_static(route, request);
    },
    [&](const DynamicRoute& route) {
        // Path contains parameters — extract bindings
        auto params = route.extract(request.path());
        serve_dynamic(route, request, params);
    },
    [&](const NotFound&) {
        serve_404(request);
    },
}, resolve(request.path()));
```

The route resolution produces a variant — `StaticRoute`, `DynamicRoute`, or `NotFound` — and `std::visit` exhaustively handles each case. Adding a new route type (say, `RedirectRoute`) forces every dispatch point to be updated.

### Node Rendering: Recursive Pattern Matching

The `render_to()` method in `dom.hpp` is recursive pattern matching on the `Node` type. The `switch` on `kind` is sum elimination, and the recursive calls on `children` descend through the fixed point:

```cpp
void render_to(std::string& out) const {
    switch (kind) {
        case Text:     escape_text(out, content); break;
        case Raw:      out += content; break;
        case Fragment:
            for (const auto& c : children) c.render_to(out);
            break;
        case Void:
            out += '<'; out += tag;
            render_attrs(out);
            out += '>';
            break;
        case Element:
            out += '<'; out += tag;
            render_attrs(out);
            out += '>';
            for (const auto& c : children) c.render_to(out);
            out += "</"; out += tag; out += '>';
            break;
    }
}
```

Five patterns, one per `Kind`. Each pattern handles its case completely. The recursive calls (`c.render_to(out)`) are the catamorphism descending through `children`. This is structural recursion guided by pattern matching — the same shape as the Haskell `map` example above, just with C++ syntax.

### Config Parsing: Nested Product + Sum Matching

Loom's configuration parser decomposes structured config values using layered pattern matching:

```cpp
auto process_value(const ConfigValue& val) {
    return std::visit(overloaded{
        [](const StringValue& s) {
            return process_string(s.content);
        },
        [](const ListValue& list) {
            std::vector<std::string> result;
            for (const auto& item : list.items) {
                // Nested match: each item is itself a ConfigValue
                std::visit(overloaded{
                    [&](const StringValue& s) { result.push_back(s.content); },
                    [&](const auto&) { /* skip non-strings in lists */ },
                }, item);
            }
            return join(result, ", ");
        },
        [](const MapValue& map) {
            return process_map(map.entries);
        },
    }, val);
}
```

The outer visit matches the top-level config value kind. The inner visit matches elements within a list. This is the nested pattern matching pattern — manual in C++, but structurally identical to deep patterns in ML or Haskell.

### Response Serialization: The Pattern That Replaced a Flag

The HTTP response variant from [part 2](/post/algebraic-data-types) is consumed by pattern matching:

```cpp
std::visit(overloaded{
    [&](const DynamicResponse& dr) { start_write_owned(fd, dr.serialize(keep_alive)); },
    [&](const PrebuiltResponse& pr) { start_write_view(fd, pr.owner, pr.data, pr.len); },
}, response);
```

Two alternatives. Two handlers. No default. If a third response type is added, this line produces a compile error. The exhaustiveness guarantee propagates the design change to every consumption site.

## Closing: Deconstruction Is Construction's Mirror

Every type has two faces. Introduction rules say how to build values. Elimination rules say how to use them. Pattern matching is the elimination side — the mirror image of constructors.

When you design a sum type, you are simultaneously designing a pattern match. Every constructor you add creates a new case that every consumer must handle. This is the contract: the producer promises to use only the declared constructors; the consumer promises to handle all of them. The compiler enforces both sides.

Get the patterns right and your code becomes a proof: every possible input is handled, every case produces the correct type, and adding a new possibility to the input automatically identifies every place in the codebase that must adapt. Forget a case, and the compiler tells you immediately — not at 3 AM when a user hits the unhandled branch.

In the [next post](/post/phantom-types), we will see how to add type-level distinctions that carry *zero data* — phantom types that leverage parametricity to make the compiler see differences that do not exist at runtime.
