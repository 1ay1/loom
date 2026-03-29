---

title: "A Program Is a Theory — Foundations of Type-Theoretic C++"
date: 2026-03-29
slug: type-theoretic-foundations
tags: [c++20, type-theory, compile-time, design, foundations]
excerpt: "Reframing C++ as a language of types and proofs, where compile time enforces the structure of the world and invalid states become unrepresentable."
---

# A Program Is a Theory of the World

There is a quiet assumption behind most C++ code:

> A program is something that runs.

That assumption is wrong — or at least incomplete.

A program does run.
But before it runs, it is *checked*.

And that checking phase is not superficial. It is structural.

It determines:

* what kinds of entities may exist
* how those entities may be constructed
* which transformations are permitted
* which combinations are forbidden

This is not execution.

This is **verification**.

So a more accurate statement is:

> A C++ program is a formal system whose consistency is checked before execution.

Or, more provocatively:

> A program is a *theory*, and the compiler is its first reviewer.

---

# The Two Languages of C++

C++ is not one language.

It is two tightly interwoven languages:

### 1. The *Type Language* (compile time)

This is where you describe:

* what exists
* what is valid
* what is distinguishable
* what is impossible

It includes:

* types
* templates
* concepts
* constant expressions
* type relationships

### 2. The *Value Language* (runtime)

This is where you describe:

* what happens
* how values flow
* how effects occur
* how the program interacts with reality

Most codebases over-invest in the second and under-invest in the first.

Type-theoretic C++ flips that balance.

---

# Types as Sets, But Also as Propositions

A useful starting point:

> A type is a set of values.

But that is only half the story.

In type theory, we also use the **propositions-as-types** idea:

> A type can represent a *statement*, and a value of that type is a *witness* (or proof) that the statement holds.

We do not get this perfectly in C++, but we get *enough* of it to matter.

Consider:

```cpp
struct NonEmptyString {
    std::string value;
};
```

This is not just “a string.”

It is a claim:

> “This string is non-empty.”

Now the important question:

Where is that enforced?

If the constructor enforces it, then:

* constructing `NonEmptyString` is proving a property
* holding a `NonEmptyString` is *having proof* of that property

This is the key shift:

> Types do not just describe shape. They encode *truths*.

---

# Illegal States Are Logical Contradictions

When we say:

> “Make invalid states unrepresentable”

we are really saying:

> “Make contradictions unconstructible.”

Take this:

```cpp
struct File {
    int fd;
    bool open;
};
```

This allows:

* `fd = -1, open = true`
* `fd = 42, open = false`

These are contradictory states.

The type permits them, so every function must defend against them.

Now consider:

```cpp
struct OpenFile {
    int fd;
};

struct ClosedFile {};
```

We have split the world into two **disjoint types**.

Now:

* an `OpenFile` *cannot* be closed
* a `ClosedFile` *cannot* be open

We removed contradiction by construction.

This is not just cleaner design.

This is **logical refinement** of the domain.

---

# Structural vs Semantic Types

Many codebases use types structurally:

```cpp
int user_id;
int order_id;
```

These are structurally identical.

The compiler sees no difference.

But semantically:

* a `user_id` is not an `order_id`
* mixing them is nonsense

Type-theoretic design insists:

> Semantic distinctions must be reflected in types.

```cpp
struct UserId { int value; };
struct OrderId { int value; };
```

Now the compiler enforces what was previously only a convention.

We have moved meaning from comments into code.

---

# The Cost of Weak Representation

Weak types push responsibility outward.

If everything is:

* `int`
* `double`
* `std::string`

then:

* every function must validate
* every boundary must re-check
* every assumption must be remembered

This creates **distributed correctness**:

Correctness is not guaranteed anywhere.

It is *hoped for everywhere*.

Strong types centralize correctness:

* validation happens once
* construction establishes invariants
* usage assumes validity

This creates **localized correctness**:

Correctness is guaranteed at construction.

---

# Typestate: Encoding Time in Types

One of the most powerful ideas we will use is **typestate**.

Instead of representing state with values:

```cpp
enum class State {
    Disconnected,
    Connected,
    Authenticated
};
```

we represent it with types:

```cpp
struct Disconnected {};
struct Connected {};
struct Authenticated {};

template<typename State>
struct Connection;
```

Now transitions become type transformations:

```cpp
auto connect(Connection<Disconnected>) -> Connection<Connected>;
auto authenticate(Connection<Connected>) -> Connection<Authenticated>;
```

You literally *cannot* authenticate before connecting.

Because:

> The function does not exist for that type.

This is a deep shift:

State is no longer *checked*.
State is *encoded*.

---

# Compile Time as a Constraint Solver

When templates, concepts, and constexpr interact, the compiler effectively becomes a constraint solver.

You provide:

* a set of types
* a set of constraints
* a set of relationships

The compiler determines:

* which instantiations are valid
* which overloads are selectable
* which programs are well-formed

Example:

```cpp
template<typename T>
concept Numeric = requires(T a, T b) {
    { a + b } -> std::same_as<T>;
};

template<Numeric T>
T add(T a, T b) {
    return a + b;
}
```

This is not just a function.

It is a **rule**:

> “Addition exists only for types that satisfy this structure.”

The compiler enforces that rule universally.

---

# Compile Time vs Runtime: A Hard Boundary

A common mistake is to treat compile time as an optimization layer.

It is not.

It is a **different phase of reasoning**.

| Compile Time    | Runtime      |
| --------------- | ------------ |
| What is allowed | What happens |
| Structure       | Behavior     |
| Possibility     | Reality      |
| Proof           | Execution    |

If something can be decided at compile time, it should be.

Because:

* compile-time errors are deterministic
* runtime errors are situational
* compile-time checks scale
* runtime checks accumulate

Or more bluntly:

> A bug caught at compile time is a bug that never existed.

---

# Types as Interfaces, Not Containers

A common misunderstanding:

> A type is something that holds data.

Better:

> A type defines what can be done.

Consider:

```cpp
struct Socket {
    int fd;
};
```

This says very little.

Compare:

```cpp
struct ConnectedSocket {
    int fd;
};

struct ListeningSocket {
    int fd;
};
```

Now:

* you cannot send on a listening socket
* you cannot accept on a connected socket

Because those operations simply do not exist for the wrong type.

This is not documentation.

This is enforcement.

---

# Zero-Cost Abstractions: Why This Works in C++

All of this would be academic if it came with heavy runtime cost.

But C++ gives us:

> Abstraction without penalty (when used correctly)

A wrapper like:

```cpp
struct UserId {
    int value;
};
```

usually compiles to the same machine code as an `int`.

Phantom types, tags, and typestate markers often disappear entirely at runtime.

So we get:

* stronger guarantees
* clearer intent
* no additional cost

This is rare among mainstream languages.

And it is why this style is worth mastering.

---

# The Design Discipline

We now arrive at the core workflow of this series.

Instead of asking:

> “How do I implement this?”

we ask:

### 1. What exists?

What are the fundamental entities?

### 2. What distinctions matter?

Which differences must the compiler understand?

### 3. What is impossible?

What must never be representable?

### 4. What are the valid transitions?

How does one valid state become another?

### 5. What can be decided at compile time?

What can be enforced before execution?

### 6. What remains for runtime?

Only what depends on external reality.

This order is everything.

---

# A Small but Complete Example

Let’s combine several ideas.

```cpp
struct RawEmail {
    std::string value;
};

struct Email {
    std::string value;
};

auto parse_email(RawEmail raw) -> std::optional<Email> {
    if (raw.value.contains('@')) {
        return Email{raw.value};
    }
    return std::nullopt;
}
```

Now:

* raw input is explicitly marked
* validated data is distinct
* invalid emails cannot exist as `Email`

Functions can now require:

```cpp
void send_email(Email email);
```

And they never need to check validity again.

We have:

* moved validation to the boundary
* encoded trust in the type
* eliminated repeated checks

---

# What This Series Will Build Toward

We are not doing theory for its own sake.

We are building toward practical systems where:

* protocols are encoded in types
* routing is computed at compile time
* state machines are enforced structurally
* APIs are impossible to misuse
* performance remains zero-cost

Future parts will cover:

* algebraic data types in C++
* typestate and protocol encoding
* compile-time parsing and validation
* concepts as logical constraints
* non-type template parameters as data
* building real systems (like Loom) with these ideas

---

# Closing Thought

Most bugs are not subtle.

They come from:

* mixing things that should not mix
* calling things at the wrong time
* forgetting to check something
* representing something too loosely

All of these are failures of *representation*.

So the goal is not:

> “Write better functions.”

The goal is:

> “Design a world where bad functions cannot exist.”

Once that world is built, everything else becomes simpler.

And the compiler — strict, pedantic, slightly judgmental — becomes your strongest ally.
