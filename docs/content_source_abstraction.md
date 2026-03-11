---
title: "Content Source Abstraction in C++"
description: "Implementing a content source abstraction using C++ concepts for zero overhead and compile-time validation."
---

The goal is for the blog engine to be agnostic to the origin of its content.

Posts could potentially come from various sources:

*   `filesystem`
*   `database`
*   `github repo`
*   `API`
*   `memory`

To achieve this, we define a `ContentSource` concept.

This is where modern type-theoretic C++ excels. Instead of using runtime polymorphism (e.g., virtual functions), we leverage compile-time interfaces with C++20 concepts.

This approach offers several advantages:

*   **Zero overhead**: No virtual table lookups or dynamic dispatch at runtime.
*   **Compile-time validation**: Type correctness and interface compliance are checked during compilation.
*   **Better optimization**: The compiler can perform more aggressive optimizations due to static knowledge of types.
