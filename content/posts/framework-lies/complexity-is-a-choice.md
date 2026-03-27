---
title: "Complexity Is a Choice: Why Your Stack Is 100x Heavier Than It Needs To Be"
date: 2026-04-10
slug: complexity-is-a-choice
tags: architecture, philosophy, systems, web
excerpt: "The modern web stack is a 100-story skyscraper built on a swamp. We keep adding floors because we're afraid to admit that the foundation is sinking."
---

We have reached a point in software history where "Hello World" requires a 500MB download.

Think about that. 

To display two words on a screen, we now use:
- A 30MB JavaScript runtime.
- A 150MB `node_modules` directory.
- A multi-stage build pipeline (Babel, Webpack, Vite).
- A CSS-in-JS library that is larger than the original browser it runs in.
- A container image that weighs 800MB.

We tell ourselves this is "modern." We tell ourselves this is "the industry standard." We tell ourselves we need this to be "productive."

**It is a mass delusion.**

Complexity is not an inevitable byproduct of software growth. It is a **choice**. And right now, the industry is choosing to drown in it.

## The `node_modules` Black Hole

The average "modern" web project starts with a deficit of 1,000+ dependencies. 

You wanted a router. You got:
- A dependency for padding strings.
- A dependency for checking if a variable is an array.
- A dependency for parsing a config file that doesn't exist yet.
- Three different versions of the same logging library.

This is **Dependency Debt**. Every single one of those 1,000 packages is a potential security vulnerability, a breaking change waiting to happen, and a piece of code you didn't write and don't understand. 

In a system like **Loom**, we have **zero** external dependencies beyond the standard library and `zlib`. The binary is 800KB. Not 800MB. **800KB.**

## The Build Step That Shouldn't Exist

We have spent the last decade building tools to fix the problems created by our previous tools.

1.  **JavaScript was slow**, so we wrote transpilers (Babel).
2.  **Files got too big**, so we wrote bundlers (Webpack).
3.  **Bundlers got too slow**, so we wrote faster bundlers in Go or Rust (esbuild, SWC).
4.  **Configuration got too hard**, so we wrote meta-frameworks to hide the config (Next.js, Nuxt).

We are sprinting on a treadmill. We are burning thousands of engineering hours every year just to keep our "modern" build pipelines from collapsing under their own weight. 

**What if you just... didn't?**

What if your code was ready to run the moment you saved the file? What if your "build" was a simple `make` command that took 10 seconds and produced a single, permanent binary?

## The "Google-Scale" Delusion

The biggest driver of complexity is **Resume-Driven Development.**

Engineers at startups with 500 users are implementing "Micro-Frontends," "Event Sourcing," and "Globally Distributed Serverless Edge Functions" because that's what Google and Netflix do.

But you are not Google. You are not Netflix.

Google uses those tools because they have 30,000 engineers and have to solve "People Problems." They need to make sure that Team A can't break Team B's code. They trade performance and simplicity for organizational safety.

When a 5-person startup makes that same trade, they aren't "building for scale." They are **committing suicide.** They are spending 80% of their time fighting their infrastructure and only 20% building their product.

## The Choice of Simplicity

Simplicity is harder than complexity. It requires saying "No."

- No, we don't need a framework for this.
- No, we don't need a distributed queue for a 1,000-user app.
- No, we don't need a 400KB CSS library to center a div.

In **Loom**, we chose to write the HTTP server, the Markdown parser, and the Template engine from scratch. Not because we have "Not Invented Here" syndrome, but because **we wanted to own the system.**

Because we own it, we can:
- Debug a request in 10 seconds.
- Run the entire site on a $5/month VPS with 99% idle CPU.
- Change any part of the behavior without waiting for a third-party maintainer to merge a PR.

## The Final Uncomfortable Truth

Most developers are afraid of simplicity.

Complexity provides a shield. If the system is complex, you have "Job Security." If the system is complex, "It's not my fault, the framework is just slow." If the system is complex, you are a "Senior Infrastructure Engineer" rather than "someone who writes code."

But the era of "Growth at any Cost" is over. The future belongs to the efficient. It belongs to the systems that can run on minimal resources, stay up for years without intervention, and be understood by a single human being.

**Complexity is a choice. It's time to choose something else.**

---

### Series: Rebuilding the Web Without Frameworks
*Part 6: Complexity Is a Choice: Why Your Stack Is 100x Heavier Than It Needs To Be*

**The End.** (Or is it? Check out the [Loom Source Code](https://github.com/1ay1/loom) to see the alternative in action.)