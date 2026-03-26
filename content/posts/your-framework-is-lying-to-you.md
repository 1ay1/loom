---
title: Your Web Framework Is Lying to You
excerpt: A blunt look at why modern web frameworks hide complexity instead of removing it.
slug: your-framework-is-lying-to-you
date: 2026-03-27
tags: systems, c++, web, architecture, performance, loom-series
---

# Your Web Framework Is Lying to You

You can build a web app in 15 minutes.

Spin up a server.
Add a few routes.
Drop in middleware.
Deploy.

It feels fast. Clean. Productive.
It feels like progress.
It is not.

## The Lie

Web frameworks do not remove complexity.
They hide it.
Hidden complexity is the most dangerous kind.
Because you do not fight it early.
You meet it later. In production. At 3AM.

## A Simple Question

Take this request:
GET /users
Now answer this, precisely:
- What exact path does this request take through your system?
- In what order does middleware execute?
- Where does memory allocation happen?
- Where can it fail?
- What is the worst-case latency?
Not approximately.
Exactly.
If you cannot answer that:
You are not running a system.
You are running a guess.

## The Illusion of Simplicity

This looks simple:
```
app.use(auth)
app.get("/users", handler)
```
But this is not a system.
It is an interface to a system you cannot see.
The real system lives in:
- middleware ordering rules
- framework internals
- implicit control flow
- undocumented behavior
You are not reading execution.
You are interpreting it.

## The Cost You Do Not See

There is a concept introduced by Fred Brooks:

- Essential complexity: part of the problem
- Accidental complexity: introduced by the solution

Frameworks claim to reduce complexity.

What they actually do is move it:

- from code to conventions
- from explicit to implicit
- from compile time to runtime

You feel faster.

But you understand less.

## Where It Breaks

### Control Flow

Control flow should be obvious.

Instead, it becomes:

- chained middleware
- next() calls
- invisible execution order

You do not follow the flow.

You reconstruct it.



### Debugging

A bug appears.

Where is it?

- in your code?
- in middleware?
- in framework internals?
- in configuration?

You start digging.

Not because the system is complex.

But because it is hidden.


### Performance

Ask a simple question:

"What does one request cost?"

Most systems cannot answer this.

Because performance was not designed.

It was discovered.


### State

Every request is a state transition.

But most frameworks do not model state.

They scatter it across:

- handlers
- middleware
- side effects

So the system behaves correctly.

Until it does not.


## The Trade

Frameworks are not useless.

They optimize for one thing extremely well:

Speed of development

But here is the trade you are making:

- You gain speed
- You lose clarity
- You lose control
- You lose the ability to reason about the system


## The Uncomfortable Truth

Most backend systems today:

- work
- scale (sometimes)
- pass tests

But cannot be fully understood by the people who built them.

That should bother you more than it does.

## A Different Direction

What if we stopped hiding the system?

What if:

- routing was a data structure, not a pattern match
- control flow was explicit, not inferred
- state transitions were modeled, not scattered
- correctness was enforced at compile time, not runtime

What if the code was the system?

Rebuilding the Web Without Frameworks

Next:

Part 2: Why Middleware Is a Broken Abstraction
