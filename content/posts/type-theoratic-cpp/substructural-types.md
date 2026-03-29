---
title: "Substructural Types — When Resources Are Not Free"
date: 2026-04-05
slug: substructural-types
tags: [c++20, type-theory, linear-types, affine-types, raii, move-semantics]
excerpt: "Classical logic lets you copy and discard premises freely. Remove those abilities and you get the logic of resources — linear, affine, relevant, ordered. RAII, move semantics, and unique_ptr are fragments of this theory. C++ is a substructural language and doesn't know it."
---

In classical logic and in most programming languages, you can use a variable as many times as you want. You can also ignore it entirely. These seem like such basic properties that they are invisible — like asking whether you can read a sentence twice.

But for *resources*, these properties are wrong. A file handle must be closed — you cannot ignore it. A database connection must not be cloned — you cannot use it twice. A message in a queue must be consumed exactly once — not zero times, not two.

**Substructural type systems** make these constraints explicit by removing the implicit "use however you want" assumption. They are the theoretical foundation for RAII, move semantics, ownership, and borrow checking. C++ has been doing substructural type theory for decades. It just did not have the vocabulary.

## The Three Structural Rules

In the sequent calculus (Gentzen, 1935), three rules govern how you use hypotheses (premises) in a proof. These are called **structural rules** because they concern the structure of proofs rather than the logical connectives:

### Weakening

If you have a valid proof using premises Γ, you can add an unused premise:

```
If  Γ ⊢ B  then  Γ, A ⊢ B
```

In programming: you can declare a variable and never use it.

```cpp
void f() {
    int x = 42;  // declared but unused
    // ... code that never mentions x ...
}   // x silently discarded — weakening in action
```

### Contraction

If you use a premise, you can use it again:

```
If  Γ, A, A ⊢ B  then  Γ, A ⊢ B
```

In programming: you can read a variable multiple times.

```cpp
void f(std::string s) {
    auto a = s.size();    // first use
    auto b = s.substr(1); // second use — contraction
}
```

### Exchange

The order of premises does not matter:

```
If  Γ, A, B ⊢ C  then  Γ, B, A ⊢ C
```

In programming: the order of variable declarations (mostly) does not affect what you can do with them.

## The Substructural Hierarchy

Substructural type systems *remove* one or more structural rules. This creates a hierarchy:

| Type System | Weakening (discard) | Contraction (copy) | Exchange (reorder) | Use Pattern |
|---|---|---|---|---|
| **Unrestricted** | ✓ | ✓ | ✓ | Any number of times, any order |
| **Affine** | ✓ | ✗ | ✓ | At most once (may discard) |
| **Relevant** | ✗ | ✓ | ✓ | At least once (may copy) |
| **Linear** | ✗ | ✗ | ✓ | Exactly once |
| **Ordered** | ✗ | ✗ | ✗ | Exactly once, in LIFO order |

Each row removes more capabilities. Let us map these to C++.

## C++ Through the Substructural Lens

### Unrestricted Types: Copy and Discard Freely

```cpp
int x = 42;
int a = x;     // copy (contraction)
int b = x;     // copy again
// x goes out of scope unused — fine (weakening)
```

Copyable, discardable. `int`, `std::string`, `std::shared_ptr<T>` — these are unrestricted types. In the substructural hierarchy, they have no usage restrictions.

In linear logic notation, an unrestricted type is `!T` — the "of course" modality. `shared_ptr<T>` is literally `!T`: a freely copyable, freely discardable handle to T.

### Affine Types: Use at Most Once

```cpp
auto p = std::make_unique<Widget>();
auto q = std::move(p);  // p is consumed (used once)
// p is now in moved-from state — using it would be a bug
// but we CAN just let q go out of scope without using it (weakening)
```

`std::unique_ptr`, `std::unique_lock`, `std::thread`, `std::future` — these are affine. You can move them (use once) or let them destruct (discard). You cannot copy them.

C++ enforces the "no copy" half of affinity through deleted copy constructors:

```cpp
std::unique_ptr<int> a = std::make_unique<int>(42);
// std::unique_ptr<int> b = a;  // ERROR: copy deleted
std::unique_ptr<int> b = std::move(a);  // OK: move
```

But C++ does NOT enforce the "must use" half. If you create a `unique_ptr` and never dereference it, the compiler does not complain — the destructor silently cleans up. True linear types would reject this.

### Relevant Types: Use at Least Once

```cpp
[[nodiscard]] auto compute_hash() -> size_t {
    return /* expensive computation */;
}

void f() {
    compute_hash();  // WARNING: ignoring nodiscard return value
}
```

`[[nodiscard]]` is a *step toward* relevant types — it warns when a return value is discarded. But it is only a warning, not an error, and it does not track whether the value is *used* after being stored. True relevant types would require every value to be consumed at least once.

### Linear Types: Exactly Once

C++ does not have true linear types. The closest approximation combines move-only types with discipline:

```cpp
class LinearHandle {
    int fd_;
public:
    explicit LinearHandle(int fd) : fd_(fd) {}
    LinearHandle(const LinearHandle&) = delete;             // no copy
    LinearHandle& operator=(const LinearHandle&) = delete;  // no copy
    LinearHandle(LinearHandle&& other) : fd_(other.fd_) { other.fd_ = -1; }

    // The "consume" operation — the only way to properly use this
    auto release() && -> int {  // && means can only be called on rvalues
        int fd = fd_;
        fd_ = -1;
        return fd;
    }

    ~LinearHandle() {
        if (fd_ >= 0) {
            // This destructor running means the handle was NOT properly consumed
            // In a true linear type system, this would be a compile error
            ::close(fd_);  // emergency cleanup
        }
    }
};
```

The `&&` qualifier on `release()` means it can only be called on rvalues — you must `std::move` the handle to consume it. But the destructor is still a safety net, not a compile-time guarantee.

**Rust** gets closer with its ownership system. The borrow checker is essentially an affine type checker. Values must be moved (not copied unless `Clone` is implemented), and the compiler verifies that moved-from values are never accessed. Rust's `Drop` trait provides cleanup (like C++ destructors), making it affine rather than truly linear.

## Girard's Linear Logic

Jean-Yves Girard introduced **linear logic** in 1987. It is not just "logic with restricted use" — it has its own set of connectives, distinct from classical logic:

### Multiplicative connectives

**A ⊗ B** (tensor / multiplicative AND): "I have one A AND one B, and I must use both."

```cpp
// A ⊗ B in C++: a pair of move-only values
auto p = std::make_pair(
    std::make_unique<FileHandle>(),
    std::make_unique<Socket>()
);
// Must use BOTH components — can't just take the file and forget the socket
```

**A ⊸ B** (lollipop / linear implication): "Consuming one A produces one B."

```cpp
// A ⊸ B in C++: a function that takes by move
auto upgrade(Connection conn) -> SecureConnection;
// conn is consumed, SecureConnection is produced
```

### Additive connectives

**A ⊕ B** (plus / additive OR): "The producer chose A or B. I must handle both."

```cpp
// A ⊕ B in C++: a variant of move-only values
using Result = std::variant<Success, Failure>;
// Must handle both cases — exhaustive visitation required
```

**A & B** (with / additive AND): "I can choose to use A or B, but not both."

This has no clean C++ analogue. It is like a value that can be "viewed" as either an A or a B, but viewing it one way destroys the other possibility. The closest thing in practice is a `union` where reading one field invalidates the other.

### Exponential modalities

**!A** (of course / bang): "Unlimited supply of A — copy and discard freely."

```cpp
// !A in C++: shared_ptr, or any copyable type
std::shared_ptr<Config> config = /* ... */;
auto a = config;  // copy — fine
auto b = config;  // copy again — fine
// discard config — also fine
```

**?A** (why not): the dual of !A. Less commonly used in programming.

The key insight: **linear logic is the logic of resources**. Every A in a linear proof must be accounted for. It cannot be duplicated (no double-free) and cannot be forgotten (no leak). The exponential `!` explicitly marks where unlimited use is allowed.

## The Resource Interpretation

A proof in linear logic is a *resource management plan*:

- Every premise is a resource that must be consumed
- Every conclusion is a resource that is produced
- Tensor (⊗) means you hold multiple resources simultaneously
- Plus (⊕) means you handle alternative outcomes
- Linear implication (⊸) means consuming a resource to produce another
- Bang (!) means a resource can be shared freely

RAII is C++'s implementation of the resource interpretation:

```cpp
{
    auto file = open("/data.txt");     // acquire resource (introduction)
    auto data = file.read();           // use resource (elimination)
    // file goes out of scope here     // release resource (automatic cleanup)
}
```

The destructor ensures the resource is released — approximating the "no weakening" rule of linear logic. Move semantics ensure the resource is not duplicated — enforcing "no contraction." Together, they give affine behavior: each resource is used at most once and cleaned up automatically.

## What C++ Gets Right

**Move semantics as affine enforcement.** Deleted copy constructors prevent duplication. `std::move` makes ownership transfer explicit. After a move, the source is in a defined-but-unspecified state — semantically consumed.

**RAII as automatic cleanup.** Destructors guarantee that resources are released. This is the "discard safely" half of affine types — you *can* discard, but the destructor handles cleanup.

**`[[nodiscard]]` as relevance hint.** Signals that a return value should not be ignored. A step toward relevant types.

**Scope-based lifetime.** Stack-allocated values have lifetimes tied to scope. This is the exchange rule in action — values are used in LIFO order, matching the "ordered" level of the substructural hierarchy.

## What C++ Gets Wrong

**Use-after-move is UB, not a type error.** The compiler does not reject `p->method()` after `std::move(p)`. Static analyzers catch some cases (`-Wuse-after-move`), but it is not a language guarantee.

```cpp
auto p = std::make_unique<int>(42);
auto q = std::move(p);
*p;  // undefined behavior — not a compile error
```

In a true affine type system (like Rust's), this would be a compile error: "use of moved value."

**Destructors always run.** Even when you want to "consume" a value into something else, the destructor still runs on the source. This makes implementing truly linear APIs awkward:

```cpp
auto conn = connect();
auto secure = upgrade(std::move(conn));
// conn's destructor runs here — even though upgrade already consumed it
// The destructor must check for the moved-from state
```

**No way to enforce "must consume."** You can create a `unique_ptr` and let it go out of scope without ever dereferencing it. A linear type system would reject this as an unused resource.

## Designing with Substructural Thinking

Even without compiler enforcement, you can design APIs that follow substructural discipline:

### Factory functions returning move-only handles

```cpp
[[nodiscard]] auto open_file(const char* path) -> std::unique_ptr<File>;
```

The caller receives an affine resource. They cannot copy it. They should not ignore it (nodiscard). They should consume it before it leaves scope.

### Methods consuming `*this`

C++23 explicit object parameters enable methods that consume the object:

```cpp
struct Connection {
    auto upgrade(this Connection self) -> SecureConnection {
        // self is taken by value — the Connection is consumed
        return SecureConnection{std::move(self.socket_)};
    }
};
```

The `this Connection self` parameter takes the object by value, consuming it. After calling `conn.upgrade()`, the `Connection` is moved-from. This is the closest C++ gets to a linear consuming method.

### Deleted operations as substructural enforcement

```cpp
class UniqueResource {
public:
    UniqueResource(const UniqueResource&) = delete;            // no contraction
    UniqueResource& operator=(const UniqueResource&) = delete; // no contraction
    UniqueResource(UniqueResource&&) = default;                // transfer OK
    ~UniqueResource();                                         // weakening with cleanup
};
```

Each deleted operation removes a structural rule.

## The Rust Comparison

Rust's ownership system is the most complete substructural type system in a mainstream language:

| Feature | C++ | Rust |
|---|---|---|
| Affine enforcement | Deleted copy ctor + move | Ownership + borrow checker |
| Use-after-move | UB / warning | Compile error |
| Must-use | `[[nodiscard]]` (warning) | `#[must_use]` (warning) |
| Shared borrowing | Raw references (unsound) | `&T` (compiler-verified) |
| Exclusive borrowing | No enforcement | `&mut T` (compiler-verified) |
| Lifetime tracking | Manual / RAII | Lifetime annotations + borrow checker |

Rust pays for this with a more complex type system and a steeper learning curve. C++ offers the same *patterns* but relies on convention rather than enforcement. The substructural theory is the same — the enforcement level differs.

## Closing: Resources Demand Discipline

Classical logic assumes premises are free — use them, copy them, discard them. This assumption is fine for mathematical propositions. It is wrong for resources. Files, connections, locks, memory, transactions — these have real costs for duplication and real consequences for neglect.

Substructural type systems formalize what systems programmers have always known: resources require discipline. Linear logic provides the theory. RAII and move semantics provide the practice. The gap between theory and practice — use-after-move being UB rather than a type error — is where bugs live.

Designing with substructural thinking means asking: for each type, which structural rules should hold?

- Can this value be copied? If no, delete the copy constructor.
- Can this value be discarded? If no, make the destructor assert or log.
- Must this value be used? If yes, add `[[nodiscard]]`.
- Should this value be consumed by a transition? If yes, take it by move.

The theory gives you the vocabulary. The practice gives you the tools. Together, they give you resource safety.

In the [next post](/post/recursive-types-and-fixed-points), we explore how recursive data structures — lists, trees, expressions — are understood through fixed points, F-algebras, and the fundamental operation of folding.
