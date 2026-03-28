---
title: "I Deleted Our Template Engine. Response Time Went From 35ms to 1ms."
date: 2026-03-14
slug: no-more-templates
tags: architecture, performance, compilers, web
excerpt: "Template engines exist so designers can edit HTML. Designers haven't touched your Jinja files in two years. You're running an interpreter inside your server for an audience that left."
---

Template engines were invented for a beautiful reason.

Separate logic from presentation. Developers write backend code. Designers edit HTML templates. Everyone does what they're good at. Clean. Clear. Professional. The kind of separation-of-concerns diagram that makes a CTO tear up a little.

Here's what actually happened: developers write the backend, then they *also* convert the Figma mockups into templates, because designers looked at `{% for item in items %}` and said "absolutely not." And they were right. They were always right. Nobody went to design school to learn Jinja syntax.

The "separation" is complete. Designers edit Figma. Developers do everything else — including maintaining a second, worse programming language embedded inside HTML files. Congratulations. You've separated concerns into "things designers do" and "literally everything."

We got all the costs of template engines. We got none of the promised benefits. We are paying rent on an apartment nobody lives in.

---

## There's An Interpreter Running Inside Your Server

When a request hits your templated endpoint, here is what happens before a single byte of HTML leaves the building:

1. The engine reads the template file from disk. (Hope the disk isn't busy.)
2. It scans the text character by character for template tags (`{{ }}`, `{% %}`, `<%= %>`).
3. It parses those tags into an Abstract Syntax Tree. Yes, a full AST. For a blog post.
4. It walks the tree, doing hash map lookups for every variable reference. Every. Single. One.
5. It concatenates the results into a string by gluing fragments together like a kindergartner with a glue stick.
6. It sends that string.

This is not "rendering HTML." This is running a programming language — a slow, interpreted, dynamically-typed programming language with no debugger, no profiler, and error messages that say things like `UndefinedError: 'dict object' has no attribute 'naem'`. You wrote "naem." You'll find out in production.

Yes, there's caching. You can cache the parsed AST. You still walk it on every render, doing type-erased lookups and dynamic string concatenation in a loop. "But we cache the AST" is like saying "but we pre-chewed the food." You still have to swallow it.

Your backend might be carefully written, compiled, optimized C++ or Rust. Profiled. Benchmarked. Tuned to within an inch of its life. And then it feeds data into a Python-adjacent string interpolation engine that allocates memory like it's going out of style. You built a Formula 1 engine and connected it to a bicycle chain.

## Where Your Types Go to Die

You have a product with a price. Your backend has:

```rust
struct Product {
    name: String,
    price_cents: i64,  // price in cents, integer, always present
}
```

Your template has:

```handlebars
<p>Price: ${{ product.price }}</p>
```

Pop quiz: what's wrong?

The field is called `price_cents`. Not `price`. The compiler cannot tell you this. The linter won't catch it. Your IDE shows no red squiggly. Your test suite probably doesn't render every template with every possible data shape. Nobody finds out until a real user loads the page and sees `Price: $` with nothing after it, like the world's most passive-aggressive price tag.

Rename `price_cents` to `price_in_cents`? Your compiler will find every call site in the backend instantly. Every single one. It's what compilers do. Your templates? That's a grep-and-pray operation, followed by manual testing of every page that might reference the field, followed by the quiet certainty that you missed one.

Every template is a hole in your type system. Every `{{ variable }}` is a runtime bet that the data will be there, in the right shape, with the right name. Your compiler checked everything up to the template boundary, shrugged, and said "good luck in there."

## The Grammar Creep

Template engines always start simple. `{{ user.name }}`. Clean. Readable. This is fine.

Then someone needs a conditional. Fine: `{% if user.is_admin %}`. Then a loop: `{% for item in cart.items %}`. Then a filter: `{{ price | format_currency }}`. Then a *custom* filter, because the built-in ones don't handle your edge case. Then a macro, because you're repeating yourself. Then a macro that takes a block. Then a macro that imports another macro.

You are now programming in Jinja. You didn't mean to. Nobody ever means to. It just happens, like mold in a bathroom.

Then the logic gets complex enough that someone puts a calculation directly in the template because it was easier than passing a pre-computed value from the backend. Now you have business logic in a string file with no debugger, no unit tests, and no compiler. It's the Wild West, except the cowboys have curly braces instead of guns, and they're equally dangerous.

Every template language eventually re-implements a programming language, poorly. The evolution is always the same: simple interpolation, then conditionals, then loops, then functions, then you're staring at a 200-line `.jinja2` file with nested conditionals and thinking "we have invented PHP again."

And PHP at least had a debugger.

## Views Are Functions

What does a template actually do, stripped of all the ceremony?

It takes data. It returns HTML.

That's a function. You already have a language for writing functions. It has a type checker, a compiler, a debugger, IDE support, first-class error messages, and decades of performance optimization behind it. It's the language your backend is already written in. You've been using it all day. It's right there.

```cpp
std::string render_product(const Product& p) {
    return "<div class='product'>"
           "<h2>" + xml_escape(p.name) + "</h2>"
           "<p>$" + format_price(p.price_cents) + "</p>"
           "</div>";
}
```

Not clever. Not exciting. Nobody's writing a conference talk about this. It's just a function that takes a struct and returns a string. The compiler verifies that `p.name` exists and is a string. The compiler verifies that `format_price` takes an `i64`. If you rename `price_cents`, every call site — including this one — breaks at compile time with an error message that tells you exactly what's wrong and where.

And it's fast. No parsing, no tree-walking, no hash map lookups, no dynamic dispatch. Just memory moving at native speed, the way CPUs have been doing since before most JavaScript frameworks were born.

When Loom's view layer was rewritten as compiled functions instead of an interpreted template pass, response time for a typical blog post dropped from 35ms to under 1ms. That's not a 2x improvement. That's a 35x improvement. Not from some breakthrough algorithm — from deleting the interpreter that shouldn't have been there in the first place.

## The Familiarity Trap

"But I can read templates at a glance. HTML with variables is natural."

You can read a paper map at a glance too. That doesn't make it better than GPS. It just means you've memorized the wrong skill.

The comfort you feel in a template language comes from years of working around its limitations. You got good at Jinja because there was nothing else. The filter syntax is familiar because you've written it hundreds of times while thinking "I wish I could just call a function." The curly braces feel natural because Stockholm Syndrome eventually does.

The alternative isn't ugly. It isn't worse. The initial 20 minutes of "this feels weird" is a small price for a view layer you can type-check, test, debug, and actually trust at 3am when the site is down and the template error message says `'NoneType' object is not iterable` with no line number.

A template engine once made sense. In 2006. When PHP was king, designers hand-edited HTML, and "separation of concerns" meant literally different people editing different files.

That world is gone. The designers are in Figma. The files are yours. You might as well write them in a real language — and pick up a free 35x speedup while you're at it.

---

**Next:** [The ORM Delusion](/post/database-lies)
