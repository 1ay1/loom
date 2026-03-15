---
title: Strong Types in C++
slug: cpp-strong-types
date: 2024-02-20
tags: cpp, type-safety
---

One of my favorite patterns in modern C++ is using strong types to prevent bugs at compile time.

## The problem

Consider a function that takes multiple string arguments:

```
void create_user(string name, string email, string phone);
```

Nothing stops you from accidentally swapping the arguments. The compiler is happy, but your users aren't.

## The solution

Wrap each type in a tagged wrapper:

```
using Name = StrongType<string, NameTag>;
using Email = StrongType<string, EmailTag>;

void create_user(Name name, Email email);
```

Now swapping arguments is a **compile error**. The type system does the work for you.

## When to use it

Strong types shine when:

- You have multiple parameters of the same underlying type
- Domain concepts deserve their own identity
- You want self-documenting code

The overhead is zero at runtime thanks to inlining. It's all compile-time safety.
