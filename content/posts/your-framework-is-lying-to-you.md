---
title: Your Web Framework Is Lying to You
date: 2026-03-27
slug: frameworks-are-lying
tags: feature, architecture, systems, web
excerpt: Web frameworks promise simplicity. What they actually do is hide complexity until it becomes your problem.
---

You can build a web app in 15 minutes.

Spin up a server.
Add a few routes.
Drop in middleware.
Deploy.

It feels fast. It feels clean. It feels productive.

It feels like progress.

It is not.

## The Lie

Web frameworks do not remove complexity.

They hide it.

And hidden complexity is the worst kind, because you do not deal with it early. You deal with it later, when it is harder, messier, and usually in production.

## A Simple Question

Take a request:

GET /users

Now answer this precisely:

- What exact path does this request take?
- In what order does middleware execute?
- Where does memory allocation happen?
- Where can it fail?
- What is the worst-case latency?

Not approximately. Exactly.

If you cannot answer that, you do not understand your system. You understand its interface.

## The Illusion of Simplicity

This looks simple:

app.use(auth)
app.get("/users", handler)

But this is not a system.

It is a surface.

The real system lives in:

- middleware ordering rules
- framework internals
- implicit control flow
- undocumented behavior

You are not reading execution. You are interpreting it.

## The Cost You Do Not See

There is a concept introduced by Fred Brooks:

- Essential complexity: part of the problem
- Accidental complexity: introduced by the solution

Frameworks claim to reduce complexity.

What they actually do is move it:

- from code to conventions
- from explicit to implicit
- from compile time to runtime

You feel faster. But you understand less.

## Where It Breaks

### Control Flow

Control flow should be obvious.

Instead, it becomes:

- middleware chains
- next() calls
- invisible execution order

You do not follow the flow. You reconstruct it.

### Debugging

A bug appears.

Where is it?

- your code
- middleware
- framework internals
- configuration

You start digging, not because the system is complex, but because it is hidden.

### Performance

Ask a simple question:

What does one request cost?

Most systems cannot answer this.

Because performance was not designed. It was observed.

### State

Every request is a state transition.

But most frameworks do not model state.

They scatter it across handlers, middleware, and side effects.

So the system works. Until it does not.

## The Trade

Frameworks are not useless.

They optimize for one thing extremely well: speed of development.

But here is the trade:

- you gain speed
- you lose clarity
- you lose control
- you lose the ability to reason about the system

## The Uncomfortable Truth

Most backend systems today:

- work
- scale sometimes
- pass tests

But cannot be fully understood by the people who built them.

## A Different Direction

What if we stopped hiding the system?

What if:

- routing was a data structure, not a pattern match
- control flow was explicit, not inferred
- state transitions were modeled, not scattered
- correctness was enforced at compile time, not runtime

What if the code was the system?

## Series

This post is part of a series: Rebuilding the Web Without Frameworks

Next:

Why Middleware Is a Broken Abstraction
