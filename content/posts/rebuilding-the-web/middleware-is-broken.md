---
title: Middleware: The Hidden Enemy of Your Web Server
date: 2026-03-12
slug: middleware-is-broken
tags: architecture, systems, series, performance
excerpt: Every call to app.use() is a promise that something, somewhere, will call next(). Whether that happens correctly is between your middleware and God.
---

At some point in your career, you will ship a bug caused by a middleware calling `next()` twice.

You won't know what happened at first. The symptoms will be surreal — a request handler running twice, a response sent twice, a database row inserted twice. Users seeing double. You'll check your handler. Clean. You'll check your route config. Perfect. You'll `console.log` the entire request lifecycle and watch, horrified, as your handler fires two times in a row with no apparent cause.

After 45 minutes of questioning your sanity, you'll find it: a third-party auth package, three levels deep in `node_modules`, maintained by someone whose GitHub bio says "crypto enthusiast and part-time DJ." There's a subtle bug in an error path where it calls `next(err)` and then, for reasons understood only by God and the original author (who are not the same person), also calls `next()`.

The framework allowed this. The framework saw nothing wrong with it. The framework, in fact, had no opinion about it whatsoever, because the framework has the architectural supervision skills of a sleeping mall cop.

---

## The `next()` Problem

The fundamental design of middleware is a function that calls another function to continue the chain:

```javascript
function myMiddleware(req, res, next) {
    // do something
    next(); // and then... what exactly?
}
```

`next()` hands control to the next item in an invisible list. There's no type signature telling you what it does. There's no compiler preventing you from calling it twice. Nothing stops you from calling it zero times, which silently kills the request and makes it vanish like a mob witness. Nothing stops you from calling it from a callback three event loop ticks later when `res` has already been sent and the client has moved on with their life.

This is **implicit control flow** — and implicit control flow is a bug that hasn't been found yet. It's Schrodinger's bug. It exists in a superposition of working and broken until someone observes it under load.

In every other context, you can read code and trace where execution goes. With middleware chains, you can't. Control doesn't flow through your code — it flows through the framework's internal list, assembled at startup from `app.use()` calls scattered across a dozen files like Horcruxes. You don't read the execution order. You reconstruct it from memory, at 3am, when something is very wrong.

## The Request Object Is a Communal Garbage Bin

The other gift middleware gives you is a shared, mutable, untyped object that every piece of code in the chain can read and write freely:

```javascript
// auth.js (file 1 of 47 in your middleware/ directory)
req.user = await lookupUser(req.headers.authorization);

// rateLimit.js (somewhere in node_modules, written in 2021, last updated never)
req.rateLimit = { remaining: 99, resetAt: Date.now() + 3600 };

// metrics.js (written by an intern who has since graduated)
req.startTime = process.hrtime();
req.requestId = uuid();

// your handler (file 48, the one you actually own)
const id = req.user_id; // where did this come from? good luck
```

`req` isn't a request anymore. It's a bathroom wall that everyone in the codebase has been writing on since the server started. There's no table of contents. There's no editor. There's just graffiti, going back years.

When your handler crashes because `req.user` is `undefined`, you don't just look at the handler. You audit every middleware in the chain to find out which one was supposed to set it, whether it ran, whether something mutated it, and whether the middleware that was supposed to run before it got added after it because someone copy-pasted an `app.use()` line to the wrong place six months ago.

This is not debugging. This is archaeology. You are an archaeologist of someone else's bad decisions, armed with `console.log` and a growing sense of dread.

Your type system has no idea what's on `req`. Your IDE can't help you. The only documentation is the code that was written by whoever had the keyboard last, and that person quit in February.

## The Performance Tax You Signed Up For Without Reading the Contract

Every `app.use()` call doesn't just add a function to a list. It adds:

1. **A closure allocation** — heap memory, every request, for every middleware. The garbage collector thanks you for the job security.
2. **An indirect function call** — the CPU can't predict where `next()` points, so it can't pipeline the execution. Your processor literally stutters because your architecture was designed for developer ergonomics, not for how silicon actually works.
3. **A full pass through the chain for every request** — your health check endpoint, the one that just returns `{"ok": true}`, runs through the same 12 middleware as your most complex authenticated endpoint. Auth. CORS. Compression. Rate limiting. Logging. All of them. For a health check.

Your `/health` route is doing a pull-up, a squat, and a mile run before it's allowed to say "I'm fine." Every 10 seconds. From every load balancer.

A request that doesn't need authentication shouldn't run authentication code. This seems obvious. It's the kind of thing you'd say and everyone would nod. Middleware architectures make it structurally difficult to achieve, because the chain runs for everything, and carving out exceptions means writing *more* middleware to skip the other middleware.

You're writing middleware to avoid middleware. Think about that.

## What Explicit Looks Like

The alternative isn't "no auth" or "no logging." It's making them visible — writing them into the routes that need them, like a person who read the requirements:

```cpp
auto handle_users(Request req) -> Response {
    auto session = Auth::validate(req);
    if (!session) return Response::unauthorized();

    Log::info("users listed", {{"user_id", session->user_id}});

    auto users = DB::query<User>("SELECT id, name FROM users");
    return Response::json(users);
}
```

Read it top to bottom. Every step is visible. Every failure path is explicit. The types enforce that you check auth before using the session — the compiler literally will not let you skip it. There's no invisible list, no shared mutable blob, no `next()` that may or may not have been called by a package you've never opened.

The auth code runs on this route because you wrote it here. It doesn't run on the health check because you didn't write it there. The reasoning is: you can read. You were always able to read. Middleware just made you forget that reading code was an option.

## The 15-Minute Demo vs. The 3-Year Production System

Middleware is excellent demo technology. You can show a secure, logged, rate-limited API in fifteen minutes by stacking five `app.use()` calls at the top of a file. It looks like architecture. It feels professional. The audience claps.

Three years later: 34 middleware functions. Nobody knows the exact order they run in. Removing any of them requires a full regression because something, somewhere, depends on `req.whatever` being set by something else. A new engineer joins and has to read all 34 before they can understand what a handler does. They don't read all 34. Nobody reads all 34. The middleware functions accumulate like sedimentary rock, each layer compressing the one below it into something nobody remembers writing.

The fifteen-minute setup bought you fifteen minutes. The maintenance costs are still compounding. They will compound forever.

The question isn't "does middleware work?" It works, for a while. Everything works for a while. The question is what you're trading for that initial convenience — and whether you'd make the same trade if the invoice arrived on Day 1 instead of Year 3.

---

*Part 2 of the "Rebuilding the Web" series.*
**Next:** [The Template Language Fallacy: Why Your View Layer is a Performance Sinkhole](/post/no-more-templates)
