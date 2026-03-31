---
title: "Parametricity — Theorems for Free"
date: 2026-03-27
slug: parametricity
tags: [c++20, type-theory, parametricity, free-theorems, polymorphism]
excerpt: "If a function works for any type and never inspects it, its behavior is constrained by the type signature alone. You get theorems without proofs. This is parametricity — and it explains why phantom types, generic algorithms, and templates are more powerful than they look."
---

In 1989, Philip Wadler published a paper called "Theorems for Free!" — with an exclamation mark that was entirely earned. The central claim sounds like a scam: just by looking at a function's *type signature*, without reading a single line of the implementation, you can deduce non-trivial facts about what it does.

Money for nothing. Theorems for free.

If someone told you they could look at the label on a shipping box — not open it, not shake it, just read the label — and tell you something true about the contents, you would reasonably suspect fraud. But Wadler was not running a con. He was building on a result by John Reynolds from 1983, and the mathematics checks out. The trick is that the label (the type signature) constrains what can be inside (the implementation) so severely that there is often only one thing it *could* be.

This result is called **parametricity**, and it is one of those ideas that makes you feel like you have been programming with a blindfold on for your entire career. It explains why generic programming works, why phantom types ([part 4](/post/phantom-types)) are safe, and why writing *less* powerful code somehow gives you *more* guarantees. If that sounds backwards, good — that is exactly the right instinct. Stay with me.

## The Core Idea

Here is the deal. If a function is polymorphic — it works for *any* type T — and it does not inspect T at runtime, then it has no choice but to treat all T uniformly. It cannot branch on what T is. It cannot conjure T values from thin air. It cannot peek inside T. It is, for all intents and purposes, blindfolded. All it can do is shuffle T values around according to the structure visible in the type signature.

You might think this makes the function useless. It is actually the opposite. This constraint is a *guarantee*. It means you — the person reading the code at 2 AM, trying to figure out why production is on fire — can reason about the function's behavior without reading its body. The type tells you what you need to know.

Think of it this way: if you hand someone a locked safe and they give you back a locked safe, you know they did not tamper with the contents. Not because you trust them, but because they did not have the combination. Parametricity is the reason polymorphic functions do not have the combination.

## The Identity Function

Let us start with the most boring function in existence:

```cpp
template<typename T>
auto id(T x) -> T;
```

What can this function do? It receives a value of some unknown type T and must hand back a value of type T. Let us consider its options:

- It cannot create a T from nothing — it has no idea what T is. Maybe T is `std::mutex`. Maybe T is some class you defined in a file nobody has opened since 2019. Maybe T does not even have a default constructor. For all this function knows, constructing a T requires a blood sacrifice and a connection to a PostgreSQL database.
- It cannot modify the T — it does not know T's interface. Maybe T has no methods. Maybe it is a lambda. Maybe it is an empty struct some intern added "for future use" three years ago.
- It cannot return a *different* T — where would it get one? Its pockets are empty. It walked into this function with one T and zero knowledge.

The **only** thing it can do is return its argument unchanged. The type signature `∀T. T → T` has exactly one inhabitant: the identity function.

Think about that for a moment. You proved a theorem — "this function must be the identity" — without reading the implementation. No tests. No code review. No squinting at a diff. No asking Dave from the platform team what it is supposed to do. The proof came entirely from the type. This is the kind of thing that makes Haskell programmers insufferable at parties, and honestly, they have earned it.

### A brief aside on "inhabitants"

When we say a type has "one inhabitant," we mean there is exactly one meaningfully distinct program that satisfies that type signature. (We are sweeping some things under the rug here — in a language with bottom/undefined, `id` could also be the function that crashes for all inputs. But crashing is the programming equivalent of flipping the table during a board game. We are talking about programs that actually do something.)

The number of inhabitants of a type is a measure of how much you know about a function from its signature alone. One inhabitant means you know everything — the type *is* the implementation. Many inhabitants means the type tells you little. This is the key insight, and we will keep coming back to it.

## Counting Inhabitants

Before we move to more free theorems, let us build some intuition about how constraining types can be. This is the part where parametricity starts to feel like magic, or at least like a very good card trick where you understand the mechanics but are still impressed.

### The constant function

```cpp
template<typename A, typename B>
auto constant(A a, B b) -> A;
```

Must return `a`. It has one A and one B. It needs to return an A. The B is useless — the function cannot convert a B into an A (it does not know what either type is). The B just sits there, taking up space on the stack like a decorative pillow on a couch. One inhabitant.

### The impossible function

```cpp
template<typename A, typename B>
auto absurd(A a) -> B;
```

This function must produce a B out of thin air, given only an A. It has no B. It cannot construct one. It cannot transform the A into a B. This type has *zero* inhabitants (among terminating programs). It is the type-theoretic equivalent of a door that leads to a brick wall.

In type theory, this is actually useful — the type `∀A B. A → B` corresponds to logical absurdity. If you ever find yourself able to implement this function without cheating, something has gone very wrong. You have either discovered a bug in the type system or a proof that mathematics is inconsistent. (In either case, update your resume.)

### The flip function

```cpp
template<typename A, typename B>
auto flip_pair(std::pair<A, B> p) -> std::pair<B, A>;
```

Must return `{p.second, p.first}`. It has one A and one B. The return type wants a B first and an A second. There is exactly one way to arrange the available pieces. One inhabitant. The implementation is forced, like a jigsaw puzzle with two pieces.

### Two arguments, same type

```cpp
template<typename T>
auto pick(T x, T y) -> T;
```

Two inhabitants: always return `x`, or always return `y`. We saw this earlier. The function cannot look at the values, so it picks by position, not by content. It is choosing between two identical sealed envelopes based purely on which hand they are in.

### The truly unconstrained

```cpp
auto mystery(int x) -> int;
```

How many inhabitants? For 32-bit integers, there are (2^32)^(2^32) possible functions — a number so large it makes the number of atoms in the observable universe look like a rounding error. You know *nothing* about this function from its type. It could add one. It could return zero. It could compute the Collatz conjecture. The type `int → int` is a blank check.

This is the spectrum. At one end, `∀T. T → T` with one inhabitant — total knowledge. At the other end, `int → int` with astronomically many inhabitants — total ignorance. Parametricity lives at the "total knowledge" end, and it gets there by making the function *less powerful*, not more.

The paradox: **you learn more from a function that can do less.**

## More Free Theorems

### Functions on pairs

```cpp
template<typename A, typename B>
auto fst(std::pair<A, B> p) -> A;
```

Same game. It must return an A. It has exactly one A available (inside the pair). It has no way to create an A or somehow transmute the B into an A — it knows nothing about either type. It is like being handed a sealed box and a sealed envelope and being told "give me back the box." You give back the box. What else are you going to do?

It must return `p.first`. Similarly, `snd` must return `p.second`. The implementations are forced.

### Functions choosing between arguments

```cpp
template<typename T>
auto choose(T x, T y) -> T;
```

This function takes two T values and returns one. It cannot create a new T. It cannot combine them — it does not know T has any operations. (Maybe T is a type whose only operation is "exist.") It must return either `x` or `y`.

But here is the fun part: its choice cannot depend on the *values* of x and y, because it cannot inspect T. It is choosing between two sealed boxes without being allowed to shake them, smell them, or hold them up to the light. So it must *consistently* pick the first argument for ALL types, or *consistently* pick the second for ALL types. There are exactly two programs with this type: "always left" and "always right." That is it. Two. For any conceivable T. For `int`, for `std::string`, for `std::vector<std::map<std::string, std::shared_ptr<std::function<void()>>>>` — the function does the same thing.

### Functions with higher-order arguments

```cpp
template<typename A, typename B>
auto apply(std::function<B(A)> f, A x) -> B;
```

This one is fun. The function has an `A`, and a function from `A` to `B`, and it must produce a `B`. There is exactly one sensible thing to do: apply `f` to `x` and return the result. The type forces your hand.

What about this one?

```cpp
template<typename A, typename B, typename C>
auto compose(std::function<C(B)> f, std::function<B(A)> g, A x) -> C;
```

It has an `A`, a function `g : A → B`, and a function `f : B → C`. It needs a `C`. The only way to get a `C` is to apply `f` to a `B`. The only way to get a `B` is to apply `g` to the `A`. So it must compute `f(g(x))`. Composition is not a design choice — it is the only option. The type has backed the implementation into a corner and said "compose, or else."

### Functions on vectors

```cpp
template<typename T>
auto rearrange(std::vector<T> v) -> std::vector<T>;
```

This function takes a vector of T and returns a vector of T. It cannot create new T values. It cannot modify existing ones. It can only rearrange — reorder, duplicate, or drop elements. It is a DJ that can remix the playlist but cannot write new songs.

Now here is where the free theorems get serious. For any function `g : A → B`:

```
map(g, rearrange(v)) == rearrange(map(g, v))
```

In English: it does not matter whether you transform the elements before or after the rearrangement. The rearrangement commutes with mapping. Whatever structural shuffling `rearrange` performs, it does the same shuffling regardless of what the elements actually are.

Let that sink in. If `rearrange` reverses the vector, then transforming-then-reversing gives the same result as reversing-then-transforming. If `rearrange` takes every other element, then transforming-then-taking-every-other equals taking-every-other-then-transforming. The *structure* of the rearrangement is independent of the *content* of the elements. The DJ's remix is the same whether the playlist is jazz or death metal.

This is the **naturality condition** from category theory — which sounds intimidating until you realize it just means "the function does not play favorites." Parametric polymorphism gives you naturality for free. No proof required. Just read the type.

### The filter-map commutation

```cpp
template<typename T>
auto my_filter(std::vector<T> v, std::function<bool(const T&)> pred)
    -> std::vector<T>;
```

The `pred` function introduces a wrinkle — it *can* inspect T values. The blindfold has a small hole. But `my_filter` itself is still parametric in T — it does not inspect the elements directly; it delegates that job to `pred`. Free theorem: for any `g : A → B`:

```
map(g, my_filter(v, pred)) == my_filter(map(g, v), pred ∘ g⁻¹)
```

...provided the predicate is compatible with the mapping. When the predicate does not depend on the specific representation, filter and map commute. The plumbing does not care about the water flowing through it.

### A theorem about map itself

Here is one Wadler highlights in the original paper. If `map` has the type:

```cpp
template<typename A, typename B>
auto map(std::function<B(A)> f, std::vector<A> v) -> std::vector<B>;
```

Then the free theorem gives you, for any `f : A → B` and `g : B → C`:

```
map(g ∘ f, v) == map(g, map(f, v))
```

Map distributes over function composition. You can fuse two maps into one. This is not just a nice property — it is a *compiler optimization*. GHC (the Haskell compiler) uses this exact free theorem to perform map fusion, eliminating intermediate data structures. The compiler gets optimization rules for free, from the type, without anyone having to write a custom optimization pass.

C++ compilers do not do this automatically (yet), but *you* can. If you know your code is parametric, you know map fusion is safe. The type told you so.

## Reynolds' Relational Interpretation

The theoretical foundation comes from John Reynolds' **relational parametricity** (1983). Reynolds was asking a deceptively simple question: what does it *mean* for a function to be polymorphic? Not "how do we implement polymorphism" (C++ has several answers to that, most of them terrifying), but "what mathematical property does a polymorphic function have?"

His answer was elegant: polymorphic functions preserve *all* relations between types. Let me unpack that.

For any polymorphic function `f : ∀T. F(T)`, and any *relation* R between types A and B:

> If you apply f to R-related inputs, you get R-related outputs.

A "relation" between types A and B is just a correspondence — a table that says which A values are "related to" which B values. You can think of it as a set of pairs (a, b) where a is from A and b is from B. The simplest example: a function `g : A → B` induces a functional relation where a is related to b if `g(a) = b`.

If this sounds abstract, it is. But the consequences are concrete.

**Example**: Take `id : ∀T. T → T` and the relation induced by `g : int → string` where `g(n) = std::to_string(n)`.

Parametricity says: if we apply `id` to both sides, related inputs give related outputs.

```
id(n) is related to id(g(n))
⟹  n is related to g(n)       // since id(x) = x
⟹  g(id(n)) = id(g(n))       // which is trivially true
```

That looks trivial for `id`, and it is. The interesting part is that this *same mechanical process* works for any polymorphic function and produces non-trivial results.

For `rearrange`:

```
g(rearrange(v)) = rearrange(g(v))   // where g is applied elementwise
```

This is the free theorem. It falls out mechanically from the type signature and the relational interpretation. No creativity required — just turn the crank. It is the mathematical equivalent of money for nothing.

### Why relations and not just functions?

You might wonder why Reynolds used *relations* instead of just functions. The answer is that relations are more general, and the extra generality is what gives you the strongest theorems. A function from A to B maps each A to exactly one B. A relation can map one A to many B's, or vice versa. This extra flexibility lets you state and prove things that function-based reasoning cannot reach.

For instance, the relational interpretation lets you prove that a parametric function cannot "count" its type parameter — it cannot behave differently based on the *number* of possible values of type T. Functions alone do not capture this constraint; relations do.

If this feels like overkill for your daily programming, it probably is. But the theorems it generates — the free theorems you use *without* thinking about relations — are the practical payoff. Reynolds did the hard math so you can reap the benefits at the type-signature level.

### The recipe for free theorems

Here is the mechanical process, step by step:

1. Start with a polymorphic type signature.
2. For each type variable `T`, introduce a relation `R` between two concrete types `A` and `B`.
3. Replace every occurrence of `T` in the signature with the relational interpretation: inputs at type `T` are R-related pairs, outputs at type `T` must be R-related pairs.
4. Simplify. When the relation is a function, the relational statement simplifies to an equation.

The result is a theorem. You did not prove it; the type proved it for you. You just read it off.

This is why Wadler called them "theorems for free." The price of admission is zero. The type does all the work. You sit back, collect your theorem, and look smug. (This is the Haskell programmer party behavior I mentioned earlier.)

## Parametricity in C++

Now for the bad news. C++ templates are *not* automatically parametric. C++ is the language that gives you the keys to every escape hatch, fire exit, and maintenance tunnel in the building, and then acts surprised when you set off the alarm.

In Haskell, polymorphic functions are parametric by default — the language simply does not give you the tools to break parametricity without going out of your way. In C++, breaking parametricity is not just possible; it is *easy*. You have to actively resist. Maintaining parametricity in C++ is like maintaining a diet at a buffet — technically possible, but the temptations are everywhere.

Here are the ways C++ breaks parametricity:

### Type inspection at compile time

```cpp
template<typename T>
auto describe(T x) -> std::string {
    if constexpr (std::is_integral_v<T>) {
        return "integer: " + std::to_string(x);
    } else if constexpr (std::is_floating_point_v<T>) {
        return "float: " + std::to_string(x);
    } else {
        return "unknown";
    }
}
```

This function is NOT parametric — it opens the sealed box, checks what is inside, and behaves differently depending on what it finds. Different types get different behavior. No free theorems apply. Wadler weeps.

`if constexpr` is incredibly useful — it is how you write efficient, type-aware generic code in C++. But every time you use it to branch on a type trait, you are trading free theorems for expressive power. That is a reasonable trade in many contexts. Just know that you are making it.

### Template specialization

```cpp
template<typename T>
struct Formatter {
    static auto format(T x) -> std::string { return "generic"; }
};

template<>
struct Formatter<int> {
    static auto format(int x) -> std::string { return std::to_string(x); }
};
```

Specialization is parametricity's evil twin. It looks similar — same template name, same interface — but behind the scenes, completely different code runs for different types. It is a master of disguise. The template *appears* to be one thing, but it is secretly many things wearing a trench coat.

Specialization is useful, it is powerful, and it demolishes every free theorem in sight.

### SFINAE and concept-based dispatch

```cpp
template<typename T>
auto to_string(T x) -> std::string
    requires requires { std::to_string(x); }
{
    return std::to_string(x);
}
```

SFINAE (Substitution Failure Is Not An Error — the greatest acronym in programming, and also the most ominous) and C++20 concepts let you conditionally enable or disable functions based on type properties. This is ad-hoc polymorphism with a friendly face. The function exists for some types and not others. Parametricity does not survive this kind of selective availability.

To be fair, this is by design. Concepts are *meant* to let you write different code for different categories of types. They are a feature, not a bug. But a feature that breaks parametricity is still a feature that breaks parametricity.

### Runtime type inspection

```cpp
template<typename T>
void inspect(T& x) {
    if (typeid(x) == typeid(int)) { /* ... */ }
}
```

`typeid` and `dynamic_cast` let you rip off the blindfold at runtime. Parametricity is not just broken — it is thrown out the window, lands on the sidewalk, and gets run over by a bus. In Haskell, the equivalent (using `Typeable`) requires explicitly requesting the capability in the type signature. In C++, it is always available. The blindfold is held on with scotch tape.

### Pointer arithmetic and reinterpret_cast

Let us not forget C++'s nuclear options:

```cpp
template<typename T>
auto sketchy(T x) -> int {
    return *reinterpret_cast<int*>(&x);  // what could go wrong
}
```

This is not just breaking parametricity — it is breaking the laws of physics. The function reaches through the type system's abstraction barrier and reads raw bytes. In a language with `reinterpret_cast`, no function is truly parametric in the formal sense. But this is also the kind of code that makes code reviewers break out in hives, so in practice, we can exclude it from polite conversation.

### When parametricity holds

Parametricity holds in C++ when you **avoid all of the above**: no `if constexpr` on type traits, no template specialization, no `typeid`, no SFINAE tricks that branch on type identity, no `reinterpret_cast`. In other words, when you actually keep the blindfold on and do not reach for the scotch tape remover:

```cpp
template<typename T>
auto swap_pair(std::pair<T, T> p) -> std::pair<T, T> {
    return {p.second, p.first};
}
```

This function is parametric. Free theorem: for any `g : A → B`:

```
map_pair(g, swap_pair(p)) == swap_pair(map_pair(g, p))
```

Swapping commutes with mapping. The function does not know what T is, so it must treat all T identically. Theorems for free — but only if you resist the temptation to peek.

Here is a useful heuristic: if your template function's body would compile identically regardless of what T is — if replacing T with `int` or `std::string` or `SomeClassYouInventedJustNow` produces the same sequence of operations — then the function is probably parametric. If the body changes based on T, it is not.

## Ad-Hoc vs Parametric Polymorphism

Christopher Strachey drew this distinction in 1967, back when most programmers were still punching cards and "compile time" meant waiting for the next ice age:

**Parametric polymorphism**: the same code for all types. A template without specialization, without type-trait branching. The behavior is uniform. Free theorems apply. The function is blindfolded and proud of it.

**Ad-hoc polymorphism**: different code for different types. Function overloading, template specialization, concept-based dispatch. The behavior varies by type. No free theorems. The function peeks and is not sorry.

The terminology is a bit unfortunate. "Ad-hoc" sounds like it was thrown together over a weekend, while "parametric" sounds like it was designed by committee. In reality, both are principled approaches — they just give you different things. Ad-hoc gives you expressiveness; parametric gives you guarantees. The choice depends on what you need.

C++ has both, intertwined like spaghetti in a colander. A `template<typename T>` with no constraints is parametric. Add `requires Printable<T>` and it becomes ad-hoc — the function now has opinions about what T can do. The transition is seamless in the syntax, which is both convenient and dangerous — you can accidentally cross the line from "free theorems" to "no free theorems" by adding a single `requires` clause.

Knowing which kind you are using matters:

```cpp
// Parametric — free theorems apply
template<typename T>
auto reverse(std::vector<T> v) -> std::vector<T>;

// Ad-hoc — no free theorems
template<typename T>
    requires std::totally_ordered<T>
auto sort(std::vector<T> v) -> std::vector<T>;
```

`reverse` must commute with map — it is blindfolded. You can transform the elements before or after reversing; the result is the same. This is not a property you need to test. It is a property you get for free.

`sort` need not commute with map — sorting integers may produce a different permutation than sorting strings, because the ordering relation differs. `sort` has taken off the blindfold, and it uses the information it sees. The price of that extra power is that you can no longer reason about `sort` from its type alone. You have to read the implementation. (Or at least the documentation. Who am I kidding — you are going to read the Stack Overflow answer.)

### The spectrum in practice

Most real-world templates live somewhere on the spectrum between pure parametric and pure ad-hoc. Consider:

```cpp
// Pure parametric — knows nothing about T
template<typename T>
auto make_pair(T a, T b) -> std::pair<T, T>;

// Slightly ad-hoc — needs T to be copyable
template<typename T>
    requires std::copyable<T>
auto duplicate(T x) -> std::pair<T, T>;

// More ad-hoc — needs T to be printable
template<typename T>
    requires Printable<T>
auto format(T x) -> std::string;

// Fully ad-hoc — specializes per type
template<typename T>
struct Serializer; // different impl for each type
```

The further down the list you go, the fewer guarantees you can derive from the type alone. `make_pair` is fully determined — there is only one thing it can do. `Serializer` is completely opaque — you must read each specialization to know what it does. The art of generic programming is choosing the right spot on this spectrum for each situation.

## Why Phantom Types Work: The Parametricity Explanation

In [part 4](/post/phantom-types), we built `Strong<Tag, T>` — a wrapper parameterized by a phantom tag that is never stored or accessed. We claimed this was safe. But "trust me" is not a proof. Parametricity is.

Code parametric in `Tag` cannot inspect the tag. Full stop. The tag exists in the type system and nowhere else. At runtime, a `Strong<PassengerTag, int>` and a `Strong<FlightTag, int>` holding the same value are *identical* — same bytes, same layout, same everything. The tag is a ghost that haunts only the compiler.

Consider:

```cpp
template<typename Tag>
auto increment(Strong<Tag, int> s) -> Strong<Tag, int> {
    return Strong<Tag, int>{s.value + 1};
}
```

What can `increment` do with `Tag`? It cannot read it — there is nothing to read. It cannot branch on it — there is nothing to branch on. It cannot use it to select different behavior — it appears in no `if constexpr`, no specialization, no concept check. The function is parametric in `Tag`, and so the free theorem applies:

For any two tags `A` and `B`, if `Strong<A, int>` and `Strong<B, int>` contain the same `int` value, then `increment` produces outputs containing the same `int` value. The function cannot distinguish tags. It cannot convert a `Strong<PassengerTag>` into a `Strong<FlightTag>`. Parametricity does not just *suggest* tag safety — it *proves* it. Mathematically. For free.

This is the formal justification behind the informal claim in part 4. Phantom types are safe not because we are careful, but because the type system makes carelessness impossible. That is a much better guarantee than "we are careful." "We are careful" is what you say right before the postmortem.

### The limits of phantom safety

Of course, parametricity only protects you within the parametric fragment. If someone writes a specialization:

```cpp
template<>
auto increment(Strong<FlightTag, int> s) -> Strong<FlightTag, int> {
    return Strong<FlightTag, int>{s.value + 100};  // flights get special treatment
}
```

...then the free theorem no longer holds. The specialization breaks parametricity, and flights get different behavior from passengers. This is why phantom types are most powerful in codebases that maintain a disciplined separation between parametric and ad-hoc code. If anyone can specialize on the tag, the tag's safety guarantee evaporates.

In Haskell, this is not a concern — you cannot specialize on a phantom parameter because the language does not let you. In C++, you can. Freedom is dangerous. With great `template<>` comes great responsibility.

## Practical Benefits of Parametric Code

### Easier to test

If a parametric function works correctly for `int`, it works for all types. Not "probably works" — *necessarily* works, by the uniformity guarantee. Your test matrix just collapsed from "every type you can think of" to "one type, maybe two if you are feeling thorough."

This is not an exaggeration. Consider testing `reverse`:

```cpp
template<typename T>
auto reverse(std::vector<T> v) -> std::vector<T>;
```

If you test `reverse` with `vector<int>` and it works, parametricity guarantees it works for `vector<string>`, `vector<double>`, `vector<MyCustomClass>`, and `vector<vector<vector<int>>>`. You do not need to test these. The type system already tested them. It tested *all of them*, simultaneously, by proving that the function's behavior cannot depend on what T is.

Compare this to testing an ad-hoc function like a serializer, where every type might have different behavior, and you need test cases for each one. Parametricity is the ultimate DRY principle applied to testing.

### Easier to optimize

The compiler can assume uniformity — no hidden branches on type identity. Inlining and specialization are straightforward because the compiler knows the same code runs for all types. The compiler is also blindfolded, and it likes it that way — fewer branches mean more predictable performance and better pipeline utilization.

More concretely, parametricity enables optimizations like:

- **Map fusion**: `map(f, map(g, v))` can be rewritten as `map(f ∘ g, v)`, eliminating an intermediate allocation. This is safe precisely because `map` is parametric — the free theorem guarantees that `map` preserves function composition.
- **Dead code elimination**: If a value of parametric type is never used, the compiler can eliminate it, knowing that the parametric code cannot have observable side effects dependent on that value's type.
- **Representation changes**: If `T` is parametric, the compiler is free to change how `T` values are stored in memory (e.g., struct layout optimization) without affecting the parametric code's behavior.

### Self-documenting

The type signature constrains behavior so tightly that the implementation is (sometimes) completely determined. `∀T. T → T` must be identity. `∀A B. (A,B) → A` must be `fst`. `∀A B C. (B → C) → (A → B) → A → C` must be function composition. The types *are* the documentation. And unlike comments, types do not lie. Comments can say "returns the first element" while the code returns the second. The type system does not permit this kind of deception.

There is a website — the Hoogle search engine for Haskell — where you can search for functions *by their type signature*. Type in `(a, b) -> a` and it finds `fst`. Type in `[a] -> [a]` and it finds `reverse`, `sort`, `tail`, `init`, and every other function with that shape. This works because types are meaningful enough to serve as documentation. In a world with parametricity, the type is often the best specification of behavior you have.

### More compositional

Parametric functions compose predictably. If `f` and `g` both commute with `map`, then `f ∘ g` commutes with `map`. Naturality is preserved under composition. It is like stacking Lego bricks — parametric pieces just snap together, and the resulting structure inherits the properties of its components.

This is why generic libraries — STL algorithms, ranges, functional combinators — tend to be parametric where possible. Each piece is simple and well-behaved on its own. When you compose them, the good behavior multiplies rather than collapsing. A pipeline of parametric transformations is as predictable as each individual step.

Ad-hoc code does not compose this gracefully. If `f` branches on the type and `g` branches on the type, then `f ∘ g` might exhibit emergent behavior that neither `f` nor `g` has individually. You have to reason about the composition case by case. It scales like O(n²) in complexity where parametric composition scales like O(n).

## The Abstraction Theorem

Reynolds' **abstraction theorem** (the formal statement of parametricity) says:

> Every term in System F (the polymorphic lambda calculus) satisfies the logical relation induced by its type.

In English: if the type system says a function is polymorphic, the function *must* behave polymorphically. There is no way to fake it. The type system does not just *describe* uniformity — it *enforces* it. The blindfold is not optional; it is bolted on.

This is a stronger statement than it might appear. It does not say "well-behaved polymorphic functions are parametric." It says *all* polymorphic functions in System F are parametric, by construction. The type system makes non-parametric behavior *inexpressible*. You cannot write a function in System F that inspects its type parameter, not because there is a rule against it, but because the language lacks the tools to do so. It is like trying to commit tax fraud in a language that has no word for money.

C++ is not System F. (If it were, compile times would be shorter and C++ programmers would be happier — though arguably less employable. Also, there would be no `reinterpret_cast`, and the world would be a slightly better place.) C++ has escape hatches: specialization, type traits, RTTI, and the dark arts of `void*`. But within the fragment of C++ that is genuinely parametric — templates without specialization, without `if constexpr` on type identity — the abstraction theorem holds. And that fragment is large enough to be useful.

### System F and C++

System F, invented independently by Jean-Yves Girard (1972) and John Reynolds (1974), is the theoretical foundation for parametric polymorphism. It extends the simply typed lambda calculus with universal quantification over types — the `∀T` in `∀T. T → T`.

C++ templates are, on the surface, similar: `template<typename T>` introduces a universally quantified type variable. But the similarity is skin-deep. In System F, type abstraction is *opaque* — a function parameterized by T genuinely cannot inspect T. In C++, type abstraction is *transparent* — you can specialize, introspect, and branch on the type to your heart's content.

This difference matters enormously for reasoning. In System F, you can derive free theorems from *any* type signature. In C++, you can derive free theorems only from type signatures whose implementations you trust to be parametric. The theorem is conditional: "if the implementation does not cheat, then..."

This is still useful. Most generic code *does not cheat*. Most templates that take an unconstrained `typename T` genuinely treat T as opaque. The assumption of parametricity is usually correct, and the free theorems derived from it are usually valid. You just cannot be *certain* without reading the code — whereas in Haskell, you can be certain without reading a single line.

## In Loom

Loom uses parametric polymorphism in several key components. Each demonstrates free theorems doing real work — not in a textbook, not in a Haskell tutorial, but in production C++ code that parses markdown files and serves web pages. If parametricity can survive contact with a static site generator, it can survive anything.

### AtomicCache<T>: Parametric in T

The `AtomicCache` in `include/loom/reload/hot_reloader.hpp` is a lock-free, atomically-swappable cache:

```cpp
template<typename T>
class AtomicCache
{
public:
    explicit AtomicCache(std::shared_ptr<const T> initial)
        : data_(std::move(initial)) {}

    std::shared_ptr<const T> load() const noexcept
    {
        return data_.load(std::memory_order_acquire);
    }

    void store(std::shared_ptr<const T> next) noexcept
    {
        data_.store(std::move(next), std::memory_order_release);
    }

private:
    std::atomic<std::shared_ptr<const T>> data_;
};
```

`AtomicCache` is parametric in `T`. It never inspects the cached value — it *cannot*, because it knows nothing about `T`. It only moves `shared_ptr<const T>` in and out. It is a glorified shelf. It does not care if you put a book on it or a cactus or a lovingly hand-crafted `SiteCache` — it holds the thing and gives it back when you ask. The shelf has no opinions about your decorating choices.

The free theorem: for any function `g : A → B`, if you transform the cached value via `g`, the cache's load/store behavior is identical. Whether it holds a `SiteCache`, a `Config`, or a `std::vector<std::string>` full of your regrets, the atomic swap semantics are the same. Parametricity guarantees that `AtomicCache` cannot corrupt or depend on the structure of what it holds.

This is not an academic nicety. It is a correctness argument. When you are writing lock-free code — the kind of code where a single misplaced memory order can cause a bug that manifests once every ten thousand requests, always on a Friday afternoon, always when the person who wrote it is on vacation — you want every guarantee you can get. Parametricity gives you one for free: the cache does not care about its contents, so the concurrency logic is independent of the data type. You can test the cache with `int` and know the concurrency behavior is identical for `SiteCache`.

### HotReloader: Parametric in the Cache Type

The `HotReloader` is generic in `Source`, `Watcher`, and `Cache`:

```cpp
template<typename Source, WatchPolicy Watcher, typename Cache>
    requires ContentSource<Source> && Reloadable<Source>
class HotReloader
{
public:
    using RebuildFn = std::function<std::shared_ptr<const Cache>(Source&, const ChangeSet&)>;
    // ...
};
```

The `RebuildFn` is parametric in `Cache`. The reloader calls `rebuild_(source_, pending)` and passes the result to `cache_.store()`. It never inspects the `Cache` value — it is a courier, not a customs inspector. It picks up the package and delivers it. It does not open the package. It does not weigh the package. It does not hold the package up to the light. Free theorem: for any transformation `g` on cache values, applying `g` before or after the reload yields the same observable behavior.

Note the asymmetry: `Source` and `Watcher` are constrained by concepts (`ContentSource`, `WatchPolicy`). Those constraints make the polymorphism *ad-hoc* for those parameters — the reloader calls `source_.reload()` and `watcher_.poll()`, which are interface-specific operations. It has taken off the blindfold for `Source` and `Watcher`. But the `Cache` parameter has no constraints beyond being a type. It is genuinely parametric, blindfold firmly in place, and the free theorem applies.

This is a nice example of the spectrum in practice. A single class template can be simultaneously parametric in some type parameters and ad-hoc in others. The free theorems apply selectively — you get them for `Cache` but not for `Source`. Knowing *which* parameters are parametric tells you *which* reasoning shortcuts are available.

### StrongType<T, Tag>: Parametric in Tag

Loom's phantom type wrapper in `include/loom/core/strong_type.hpp`:

```cpp
template<typename T, typename Tag>
class StrongType
{
public:
    explicit StrongType(T value) : value_(std::move(value)) {}
    T get() const { return value_; }
    bool operator==(const StrongType& other) const { return value_ == other.value_; }
    // ...
private:
    T value_;
};
```

`StrongType` is parametric in `Tag`. The tag is never stored, never inspected, never used at runtime. It is a ghost — present in the type system, absent from the binary. At runtime, `StrongType<std::string, SlugTag>` and `StrongType<std::string, TitleTag>` are byte-for-byte identical. The tag exists only in the compiler's imagination, which is exactly where we want it — the compiler has a much better imagination than we do, and it never forgets.

The free theorem: for any two tags `A` and `B`, if `StrongType<std::string, A>` and `StrongType<std::string, B>` hold the same string, then every tag-polymorphic operation produces the same underlying result. Tag-polymorphic code cannot distinguish a `Slug` from a `Title` — it must treat them uniformly.

This is the parametricity guarantee from the phantom types discussion, now with formal backing. `StrongType` never branches on `Tag`, never specializes for `Tag`, never uses `if constexpr` to inspect it. The code is genuinely parametric, the abstraction theorem applies, and a `Slug` cannot be silently converted to a `Title`. Not because of discipline. Not because of code review. Because of math.

## The Deeper Connection: Information Hiding

There is a deeper principle at work here that connects parametricity to one of the oldest ideas in software engineering: information hiding. David Parnas argued in 1972 that modules should hide their implementation details behind interfaces. Parametricity is the type-theoretic version of this idea, taken to its logical extreme.

A parametric function hides *everything* about its type parameter. It does not just hide the implementation — it hides the identity of the type itself. The function knows the type exists and that is it. This is information hiding so thorough that it becomes a source of theorems.

Consider the analogy to abstract data types. When you use an ADT through its interface, you cannot depend on the internal representation. This means the implementor can change the representation without breaking your code. Parametricity provides the same guarantee at the function level: because a parametric function cannot depend on the type T, the caller can substitute *any* T without breaking the function.

This is also why parametric polymorphism and encapsulation play so well together. A generic container like `std::vector<T>` is a parametric container — it does not know what it holds — wrapped in an encapsulated interface. The parametricity guarantees that the container treats all elements uniformly. The encapsulation guarantees that users cannot mess with the container's internals. Together, they provide a double layer of protection: the container cannot cheat on the elements, and the users cannot cheat on the container.

## Closing: Constraints as Theorems

The usual view of types is that they are restrictive — types prevent you from doing things, like a safety rail that exists only to annoy you. Parametricity flips this upside down. Types do not just prevent errors — they *prove properties*. The more polymorphic a function's type, the less it can do, and therefore the more you know about what it *does* do.

A function `∀T. T → T` is maximally constrained and maximally known — it can only be the identity. A function `int → int` is minimally constrained and minimally known — it could be any of the roughly four billion functions on 32-bit integers. Good luck reasoning about that. Good luck testing it. Good luck explaining it to the person who maintains it after you leave.

This is the paradox of polymorphism: **the less a function can do, the more you know about it.** Generality is not vagueness — it is precision. And that precision comes for free, directly from the type. No proofs, no tests, no trust falls. Just types.

The next time you write a template and are tempted to add `if constexpr (std::is_integral_v<T>)`, pause for a moment. Ask yourself: do I need this? Every type inspection you add is a free theorem you lose. Sometimes the inspection is necessary — you need different behavior for different types, and that is fine. But sometimes you can keep the blindfold on, let parametricity do its work, and get theorems for free.

It is, after all, the best price for a theorem you will ever find.

In the [next post](/post/substructural-types), we explore what happens when we restrict not just *what* you can do with values, but *how many times* — substructural types, linear logic, and the formal theory behind RAII and move semantics.
