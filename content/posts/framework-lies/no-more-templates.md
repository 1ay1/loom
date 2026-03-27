---
title: "The Template Language Fallacy: Why Your View Layer is a Performance Sinkhole"
date: 2026-03-27T01:02:00
slug: no-more-templates
tags: architecture, performance, compilers, web
excerpt: Template engines exist so that designers can edit HTML. Designers haven't touched your Jinja files in two years.
---

Template engines were invented for a beautiful reason.

Separate logic from presentation. Developers write backend code. Designers edit HTML templates. Everyone does what they're good at. Clean, clear, modern.

Here's what actually happens: developers write the backend, then they *also* convert the Figma mockups into templates, because designers don't know what `{% for item in items %}` means and shouldn't have to. The "separation" is complete. Designers edit Figma. Developers do everything else — including maintaining a second, worse programming language embedded in the HTML files.

We got all the costs of template engines. We got none of the promised benefits.

---

## There's An Interpreter Running Inside Your Server

When a request hits your templated endpoint, here is what happens before a single byte of HTML goes out:

1. The engine reads the template file from disk.
2. It scans the text for template tags (`{{ }}`, `{% %}`, `<%= %>`).
3. It parses those tags into an Abstract Syntax Tree.
4. It walks the tree, doing hash map lookups for every variable reference.
5. It concatenates the results into a string.
6. It sends that string.

This is not "rendering HTML." This is running a programming language — a slow, interpreted, dynamically-typed programming language — on every single request, even when the output is identical to the last thousand requests.

Yes, there's caching. You can cache the parsed AST. You still walk it on every render, doing type-erased lookups and dynamic string concatenation in a loop. The CPU does not enjoy this.

Your backend might be carefully written, compiled, optimized C++ or Rust. And then it feeds data into a Python-adjacent string interpolation engine and waits.

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

The compiler cannot tell you that `price` doesn't exist on `Product` — only `price_cents` does. The linter won't catch it. Your IDE shows no red squiggly. Nobody finds out until a user loads the page and sees `Price: $` with nothing after it, or until the template engine throws a runtime error and someone gets paged.

Rename `price_cents` to `price_in_cents`? Your compiler will find every call site in the backend immediately. Your templates? That's a grep-and-pray operation, followed by manual testing of every page that might reference the field.

Every template is a hole in your type system. Every `{{ variable }}` is a runtime bet that the data will be there, in the right shape, with the right name. Your compiler checked everything up to the template boundary and then washed its hands of the whole affair.

## The Grammar Creep

Template engines always start simple. `{{ user.name }}`. Clean. Readable.

Then someone needs a conditional. Fine: `{% if user.is_admin %}`. Then a loop. `{% for item in cart.items %}`. Then a filter. `{{ price | format_currency }}`. Then a *custom* filter, because the built-in ones aren't enough. Now you're writing Python inside your HTML files.

Then the logic gets complex enough that someone puts a calculation directly in the template because it was easier than passing a pre-computed value from the backend. Now you have business logic in a string file with no debugger, no unit tests, no type system, and no compiler to tell you when you break it.

Every template language eventually re-implements a programming language, but worse. The endpoint is always the same: `.jinja2` files with 200 lines of conditional logic that nobody wants to touch.

## Views Are Functions

What does a template actually do?

It takes data. It returns HTML.

That's a function. You already have a language for writing functions. It has a type checker, a compiler, a debugger, IDE support, first-class error messages, and decades of performance optimization. It's the language your backend is already written in.

```cpp
std::string render_product(const Product& p) {
    return "<div class='product'>"
           "<h2>" + xml_escape(p.name) + "</h2>"
           "<p>$" + format_price(p.price_cents) + "</p>"
           "</div>";
}
```

Not clever. Not exciting. A function that takes a struct and returns a string. The compiler verifies that `p.name` exists and is a string. The compiler verifies that `format_price` takes an `i64`. If you rename `price_cents`, every call site — including this one — breaks at compile time.

And it's fast. No parsing, no tree-walking, no hash map lookups. Just memory, moving around at native speed, the way CPUs have been doing since before most of us were born.

When Loom's view layer was rewritten as compiled functions instead of an interpreted template pass, response time for a typical blog post dropped from 35ms to under 1ms. Not from any clever optimization — from eliminating the interpretation overhead that was there for no good reason.

## The Familiarity Trap

"But I can read templates at a glance. HTML with variables is natural."

You can read a paper map at a glance too. That doesn't make it better than GPS.

The comfort you feel in a template language comes from years of working around its limitations. You got good at Jinja because you had no alternative. The filter syntax is familiar because you've written it hundreds of times while wishing you could just call a function. The perceived naturalness is learned helplessness with good branding.

The alternative isn't ugly. String builders are readable. Native control flow is cleaner than `{% if %}...{% endif %}`. The initial 20 minutes of "this feels weird" is a small price for a view layer you can type-check, test, and actually trust at 3am when the site is down.

---

*Part 3 of the "Rebuilding the Web" series.*
**Next:** [The ORM Delusion: Why Your Database Abstraction Is Making You Slow](/post/database-lies)
