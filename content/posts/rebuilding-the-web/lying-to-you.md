---
title: Your Web Framework Is Lying to You
date: 2026-03-10
slug: frameworks-are-lying
tags: feature, architecture, systems, web
excerpt: Your framework didn't remove complexity. It hid it somewhere you can't find at 3am when everything is on fire.
---

Here is the modern web developer onboarding experience:

1. Install Node.js.
2. `npm create my-app`
3. Watch 847 packages download. Wonder briefly if any of them are bitcoin miners.
4. Open the project.
5. Stare at 14 config files you didn't ask for.
6. Google "what does vite.config.js actually do."
7. Close the tab because the answer was longer than the file.
8. Give up and write the feature.

Six months later, production catches fire. You don't know why. You don't know where to start looking. You've never actually understood what the project *does* — only what it *outputs*. You are a very well-paid typist operating machinery you do not understand.

This is not an accident. This is what frameworks are designed to produce.

---

## The Pitch vs. The Reality

Every framework sells the same dream:

> *"Convention over configuration. Build in minutes, not months. Focus on your business logic, not the plumbing."*

Beautiful pitch. Gorgeous marketing. The plumbing still exists — it just lives inside the framework now, where you can't see it, can't reason about it, and can't fix it when it explodes at 2am on a Saturday while you're three beers deep into a barbecue.

What they're really saying is: *trust us to understand your system for you.*

This is like trusting a stranger to pack your parachute. For a while, that's fine. Until the ground is approaching very quickly and you can't find the ripcord because someone abstracted it behind a plugin system.

## One Simple Request

You have this code:

```javascript
app.use(auth);
app.use(rateLimiter);
app.use(logger);
app.get("/users", getUsers);
```

Four lines. A child could read it. Now answer these questions:

- In what order does each function execute?
- What happens if `auth` calls `next()` twice?
- Does `logger` still run if `rateLimiter` throws?
- What memory gets allocated, and who frees it?
- What is the worst-case latency for this single operation — precisely?

Not roughly. Not "probably under 100ms." *Exactly.*

If you can answer all five without opening the framework's source code, you're either a framework maintainer or you're confidently wrong about at least two of them. There is no third option.

Most developers can't answer these questions. Which is fine — until the system is under real load, something breaks mysteriously, and you're staring at a stack trace that leads into framework internals you've never seen before. At that point you are not debugging. You are spelunking.

## What Frameworks Actually Do to Complexity

Fred Brooks drew a line between two kinds of complexity:

- **Essential complexity:** The inherently hard parts of the problem you're solving.
- **Accidental complexity:** The hard parts introduced by the solution you chose.

Frameworks are accidental complexity delivery machines. They don't eliminate the complexity of building a web server — they relocate it. From your code, where you can see it, into the framework's intestines, where you can't.

Think of it as cleaning your room by shoving everything into the closet. The room looks great. You feel productive. Then one day you need your passport and you open the closet door and an avalanche of junk buries you alive.

The trade feels great until you need to debug something that lives in the closet.

## The Two Kinds of Bugs

**Type 1:** You read the code. You find the bug. You fix it. You go home. You sleep well.

**Type 2:** You read the code. No bug. You read the framework code. Still no bug. You read the framework's dependency's code. Nothing obvious. You find a GitHub issue from 2019 where a maintainer explains that middleware X and middleware Y interact badly when Y calls `next()` asynchronously under a specific condition that matches your production traffic pattern exactly. The fix is adding a comment that says `// DO NOT CHANGE THE ORDER OF THESE`. You commit it at 4am. You do not sleep well.

Frameworks are Type 2 bug factories. They take simple problems and shove them into layers you don't own, maintained by people who've never seen your code, governed by conventions you read once in a tutorial three years ago.

## The 40-Hour Loan

Frameworks are not a gift. They are a loan with compound interest.

**Month 1:** The framework saves you 40 hours of boilerplate. You tell your manager about the incredible velocity. You are a genius.

**Month 6:** You spend 10 hours debugging a middleware interaction the documentation doesn't mention. You start to suspect you are not a genius.

**Year 1:** A new engineer joins, spends three weeks "learning the framework" before they write a single line of business logic. You realize onboarding now means "memorizing someone else's opinions about software."

**Year 2:** A major version upgrade takes two sprints and breaks three things you didn't know were connected. One of them is auth. The CEO sends a Slack message. It is not congratulatory.

The interest compounds. The initial savings evaporate. What you're left with is a system inside a black box, maintained by strangers whose priorities are definitely not yours, running on conventions you've half-forgotten, deployed on a Friday by someone who was in a hurry.

## What Understanding Your System Actually Means

Here's a test. Take any request your system handles.

Can you trace — from memory — the exact execution path? Every function call, every possible failure mode, every allocation?

If yes: you understand your system.

If no: you have a *working* system.

These are not the same thing. Working systems become not-working systems on a schedule: when traffic doubles, when a dependency breaks, when a new engineer changes something they thought was safe. Understanding is the only insurance that actually pays out.

## A Different Way to Think About It

Routing is a data structure. Control flow is logic. State transitions can be typed and verified.

None of that requires magic. None of it requires a framework that hides what's happening behind seven layers of middleware and a prayer. The code *can be* the system — not a facade painted on top of one.

That's what the rest of this series is about. We're going to look at every piece of the modern web stack — middleware, templates, ORMs, microservices — and ask a question the framework vendors don't want you to ask:

*What if the reason this is hard is because someone made it hard on purpose?*

---

**Next:** [app.use(chaos): Why Middleware Architectures Fall Apart](/post/middleware-is-broken)
