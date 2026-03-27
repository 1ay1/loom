---
title: Your Web Framework Is Lying to You
date: 2026-03-27T01:00:00
slug: frameworks-are-lying
tags: feature, architecture, systems, web
excerpt: Your framework didn't remove complexity. It hid it somewhere you can't find at 3am when everything is on fire.
---

Here is the modern web developer onboarding experience:

1. Install Node.js.
2. `npm create my-app`
3. Watch 847 packages download.
4. Open the project.
5. Stare at 14 config files.
6. Google "what does vite.config.js actually do."
7. Give up and write the feature.

Six months later, something breaks in production. You don't know why. You don't know where to start looking. You've never actually understood what the project *does* — only what it *outputs*.

This is not an accident. This is what frameworks are designed to produce.

---

## The Pitch vs. The Reality

Every framework sells the same dream:

> *"Convention over configuration. Build in minutes, not months. Focus on your business logic, not the plumbing."*

Good pitch. The plumbing still exists — it just lives inside the framework now, where you can't see it, can't reason about it, and can't fix it when it explodes at 2am.

What they're really saying is: *trust us to understand your system for you.*

For a while, that's fine. Until it isn't.

## One Simple Request

You have this code:

```javascript
app.use(auth);
app.use(rateLimiter);
app.use(logger);
app.get("/users", getUsers);
```

Simple question: exactly what happens when `GET /users` comes in?

- In what order does each function execute?
- What happens if `auth` calls `next()` twice?
- Does `logger` still run if `rateLimiter` throws?
- What memory gets allocated, and who frees it?
- What is the worst-case latency for this single operation — precisely?

Not roughly. *Exactly.*

If you can answer all of that without opening the framework's source code, you're either a framework maintainer or you're wrong about at least one of them.

Most developers can't answer these questions. Which is fine — until the system is under real load, something breaks mysteriously, and you're staring at a stack trace that leads into framework internals you've never seen before.

## What Frameworks Actually Do to Complexity

Fred Brooks drew a line between two kinds of complexity:

- **Essential complexity:** The inherently hard parts of the problem you're solving.
- **Accidental complexity:** The hard parts introduced by the solution you chose.

Frameworks are accidental complexity delivery machines. They don't eliminate the complexity of building a web server — they relocate it from your code (where you can see it) into the framework's internals (where you can't).

The trade feels great until you need to debug something that lives in the relocated part.

## The Two Kinds of Bugs

**Type 1:** You read the code. You find the bug. You fix it. Done.

**Type 2:** You read the code. No bug. You read the framework code. Still no obvious bug. You find a GitHub issue from 2019 where a maintainer explains that middleware X and middleware Y interact badly when Y calls `next()` asynchronously under a specific condition — the exact condition your production traffic hits constantly. You fix it by adding a comment that says `// DO NOT CHANGE THE ORDER OF THESE`.

Frameworks are optimized for creating Type 2 bugs. They take simple problems and push the solution into layers you don't own and can't easily inspect.

## The 40-Hour Loan

Frameworks are a loan, not a gift.

**Month 1:** The framework saves you 40 hours of boilerplate.

**Month 6:** You spend 10 hours debugging a middleware interaction the documentation doesn't mention.

**Year 1:** A new engineer joins, spends three weeks "learning the framework" before they can contribute anything.

**Year 2:** A major version upgrade takes two weeks and breaks three things you didn't know were connected.

The interest compounds. The initial savings disappear. What you're left with is a system that exists inside a black box, maintained by strangers whose priorities may not be yours, running on conventions you've half-forgotten.

## What Understanding Your System Actually Means

Here's a test. Take any request your system handles.

Can you trace — from memory — the exact execution path? Every function call, every possible failure mode, every allocation?

If yes: you understand your system.

If no: you have a *working* system.

These are not the same thing. Working systems become not-working systems when traffic doubles, when a dependency breaks, when a new engineer changes something they thought was safe. Understanding is the only protection that actually works.

## A Different Way to Think About It

Routing is a data structure. Control flow is logic. State transitions can be typed and verified.

None of that requires magic. None of it requires a framework that hides what's happening. The code *can be* the system — not a facade on top of one.

That's what the rest of this series is about.

---

*Part 1 of the "Rebuilding the Web" series.*
**Next:** [Middleware: The Hidden Enemy of Your Web Server](/post/middleware-is-broken)
