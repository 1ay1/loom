---
title: "Middleware: The Hidden Enemy of Your Web Server"
date: 2026-03-27T01:01:00
slug: middleware-is-broken
tags: architecture, systems, series, performance
excerpt: Every call to app.use() is a promise that something, somewhere, will call next(). Whether that happens correctly is between your middleware and God.
---

At some point in your career, you will ship a bug caused by a middleware calling `next()` twice.

You won't know what happened at first. The symptoms will be strange — a request handler running twice, a response being sent twice, a database row inserted twice. You'll check your handler. It's fine. You'll check your route config. Fine. After 45 minutes of staring at nothing, you'll find it: a third-party auth package, buried three levels deep in `node_modules`, with a subtle bug in an error path where it calls `next(err)` and then, for reasons known only to the author, also calls `next()`.

The framework allowed this. The framework had no opinion about it.

---

## The `next()` Problem

The fundamental design of middleware is a function that calls another function to continue the chain:

```javascript
function myMiddleware(req, res, next) {
    // do something
    next(); // and then... what exactly?
}
```

`next()` hands control to the next item in an invisible list. There's no type signature. There's no compiler check. Nothing prevents you from calling it twice, not calling it at all, or calling it from a callback three event loop ticks later when `res` has already been sent.

This is **implicit control flow** — and implicit control flow is a bug that hasn't been found yet.

In every other context, you can read code and trace where execution goes. With middleware chains, you can't. Control doesn't flow through your code — it flows through the framework's internal list, constructed at startup from calls to `app.use()` scattered across a dozen files. You don't read it. You reconstruct it, when something breaks.

## The Request Object Is a Communal Garbage Bin

The other gift middleware gives you is a shared, mutable, untyped blob that every piece of code in the chain can read and write:

```javascript
// auth.js (file 1 of 47 in your middleware/ directory)
req.user = await lookupUser(req.headers.authorization);

// rateLimit.js (somewhere in node_modules)
req.rateLimit = { remaining: 99, resetAt: Date.now() + 3600 };

// your handler (file 48, the one you actually own)
const id = req.user_id; // where did this come from? time to grep
```

`req` isn't a request anymore. It's a sticky note that everyone in the codebase has been writing on since the server started.

When your handler crashes because `req.user` is `undefined`, you don't just look at the handler. You audit every middleware file to find out which one was supposed to set it, whether it ran, and whether something mutated it before it reached you. This is not debugging. This is archaeology.

Your type system has no idea what's on `req`. Your IDE can't help. The only documentation is code written by whoever had the keyboard last.

## The Performance Tax You Signed Up For Without Reading the Contract

Every `app.use()` call doesn't just add a function to a list. It adds:

1. **A closure allocation** — heap memory, every request, for every middleware.
2. **An indirect function call** — the CPU can't predict where `next()` points, so it can't pipeline the execution.
3. **A full pass through the chain for every request** — your health check endpoint runs through the same 12 auth/cors/compression checks as every other route, because the framework doesn't differentiate.

A simple benchmark tells the story: the per-request overhead of a 10-middleware chain is measurable. On a hot path handling thousands of requests per second, it accumulates into real latency. The auth code runs on routes that don't need auth. The CORS headers get computed for requests that never needed them.

A request that doesn't need authentication shouldn't run authentication code. This seems obvious. Middleware architectures make it structurally difficult to achieve.

## What Explicit Looks Like

The alternative isn't "no auth" or "no logging." It's making them explicit — writing them into the routes that need them:

```cpp
auto handle_users(Request req) -> Response {
    auto session = Auth::validate(req);
    if (!session) return Response::unauthorized();

    Log::info("users listed", {{"user_id", session->user_id}});

    auto users = DB::query<User>("SELECT id, name FROM users");
    return Response::json(users);
}
```

Read it top to bottom. Every step is there. Every failure path is explicit. The types enforce that you check auth before using the session. There's no invisible list, no shared mutable blob, no `next()` that may or may not have been called by a package you've never read.

The auth code runs on this route because you wrote it here. It doesn't run on the health check because you didn't write it there. The compiler knows exactly where execution goes — no indirection, no surprise, no 45-minute debugging session on a Tuesday morning.

## The 15-Minute Demo vs. The 3-Year Production System

Middleware is excellent demo technology. You can show a secure, logged, rate-limited API in fifteen minutes by stacking five `app.use()` calls at the top of a file. It looks like architecture. It's satisfying.

Three years later: 34 middleware functions, nobody knows the exact order they run in, removing any of them requires full regression because something depends on `req.whatever` being set by something else, and onboarding a new engineer means they have to read every single one before they can understand what a handler actually does.

The fifteen-minute setup bought you fifteen minutes. The maintenance costs are still compounding.

The question isn't "does middleware work?" It works, for a while. The question is what you're trading for that initial convenience — and whether you'd make the same trade if the invoice arrived before the demo.

---

*Part 2 of the "Rebuilding the Web" series.*
**Next:** [The Template Language Fallacy: Why Your View Layer is a Performance Sinkhole](/post/no-more-templates)
