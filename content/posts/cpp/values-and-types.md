---
title: "Values, Types, and const — The Foundation of Everything"
date: 2025-08-01
slug: values-and-types
tags: cpp, fundamentals, types
excerpt: Every C++ program is a conversation between values and types. Learn the language they speak.
---

If you want to understand C++, forget classes for a moment. Forget templates. Forget everything that makes people afraid of the language. Start with the thing that makes C++ *C++*: the relationship between values and types.

Every variable in C++ has a type, and that type is known at compile time. This is not a limitation. This is the single most powerful feature of the language, because it means the compiler can catch your mistakes before your code ever runs, and it can generate machine code that is exactly as fast as if you'd written it by hand in assembly.

## Fundamental Types

C++ gives you a small set of built-in types. Here are the ones you'll actually use:

```cpp
int count = 42;           // signed integer, at least 16 bits (usually 32)
bool active = true;       // true or false, nothing else
double ratio = 3.14;      // 64-bit floating point
char letter = 'A';        // single character (1 byte)
```

Then there's `std::string`, which is not a built-in type but lives in the standard library. You'll use it constantly:

```cpp
#include <string>

std::string title = "Values, Types, and const";
std::string empty;  // default-constructed: ""
```

And `size_t`, which is the type the standard library uses for sizes and indices. It's an unsigned integer that's guaranteed to be large enough to represent the size of any object in memory:

```cpp
#include <cstddef>

size_t length = title.size();  // .size() returns size_t
```

Use `int` for general numbers, `size_t` for sizes and indices. That's the convention, and fighting it will cause you nothing but pain with signed/unsigned comparison warnings.

## const: The Most Important Keyword

Here is my most opinionated claim about C++: `const` is the most underrated keyword in any programming language. In a language where you can mutate anything, the ability to say "this value will never change" is what makes large programs comprehensible.

```cpp
const int max_events = 256;               // compile-time constant
const std::string site_name = "My Blog";  // runtime constant
```

When you see `const`, you know: this will not change. You can reason about it. You can hold it in your head without worrying that some function three call stacks deep is going to mutate it out from under you.

In Loom, the blog engine this site runs on, constants appear everywhere:

```cpp
static constexpr size_t MAX_REQUEST_SIZE = 1024 * 1024;
static constexpr int MAX_EVENTS = 256;
static constexpr int KEEPALIVE_TIMEOUT_MS = 5000;
```

The `constexpr` modifier is even stronger than `const` — it means the value is known at compile time. The compiler can fold it directly into the generated code. We'll explore `constexpr` deeply in a later post; for now, just know it exists.

## Variables and auto

C++ lets you declare variables with explicit types or let the compiler infer the type with `auto`:

```cpp
std::string title = "Hello";     // explicit
auto title = std::string("Hello"); // inferred — same result

int count = 10;    // explicit
auto count = 10;   // inferred as int
```

When should you use `auto`? When the type is obvious from context or when it would be verbose to spell out:

```cpp
auto it = my_map.find("key");  // vs std::unordered_map<std::string, int>::iterator
auto posts = engine.list_posts();  // vs std::vector<PostSummary>
```

When should you spell out the type? When it helps the reader understand what's happening:

```cpp
std::string_view path = request.path;  // important: this is a view, not an owning string
int port = 8080;                        // obvious, no need for auto
```

The guideline is: use `auto` when the type is noise, spell it out when the type is signal.

## References: Borrowing Without Copying

A reference is an alias for an existing value. It does not copy. It does not allocate. It's just another name for the same thing:

```cpp
std::string name = "Loom";
std::string& ref = name;   // ref IS name — same object
ref = "Loom v2";            // modifies name
// name is now "Loom v2"
```

A `const` reference is a read-only alias:

```cpp
const std::string& cref = name;  // can read, cannot modify
// cref = "nope";                 // compile error
```

This is the most common parameter type in C++. When you see a function that takes `const std::string&`, it means: "I want to look at your string but I promise not to change it, and I promise not to copy it."

## Pass by Value vs. Pass by Reference

This is where C++ diverges from languages like Python or JavaScript. In those languages, objects are always passed by reference (sort of). In C++, you choose:

```cpp
// Pass by value: copies the string
void print_value(std::string s) {
    // s is a copy — modifying it doesn't affect the caller
}

// Pass by const reference: borrows the string
void print_ref(const std::string& s) {
    // s is an alias — no copy, but you can't modify it
}

// Pass by mutable reference: borrows and can modify
void append_to(std::string& s) {
    s += " (modified)";  // caller sees the change
}
```

The rule of thumb: pass small types (`int`, `bool`, `double`, pointers) by value. Pass everything else by `const&` unless you need to modify it, in which case pass by `&`.

Look at how Loom's blog engine does it:

```cpp
class BlogEngine {
public:
    explicit BlogEngine(const Site& site)
        : site_(site) {}

    std::vector<PostSummary> posts_by_tag(const Tag& tag) const;
    // ...
private:
    const Site& site_;
};
```

The constructor takes `const Site&` — it borrows the site, doesn't copy it. The `posts_by_tag` method takes `const Tag&` — borrows the tag. The method itself is marked `const`, meaning it promises not to modify the BlogEngine's state. Every `const` here is a contract, enforced by the compiler.

## Structs: Grouping Data Together

A struct is a bundle of named values:

```cpp
struct Post {
    PostId id;
    Title title;
    Slug slug;
    Content content;

    std::vector<Tag> tags;
    std::chrono::system_clock::time_point published;

    bool draft = false;
    std::string excerpt;
    int reading_time_minutes = 0;
};
```

This is Loom's actual `Post` struct. Notice several things:

First, the types aren't raw strings — they're `PostId`, `Title`, `Slug`, `Content`, `Tag`. These are strong types (wrappers around `std::string` that prevent you from accidentally passing a slug where a title is expected). We'll cover how they work in the templates post.

Second, some fields have default values (`draft = false`, `reading_time_minutes = 0`). This means you can construct a Post without specifying every field.

Third, the struct has no methods. It's pure data. In C++, this is often the right design. Not everything needs to be an object with behavior. Sometimes data is just data.

Here's the `Site` struct that holds the entire blog:

```cpp
struct Site {
    std::string title;
    std::string description;
    std::string author;
    std::string base_url;

    std::vector<Post> posts;
    std::vector<Page> pages;
    Navigation navigation;
    Theme theme;
    Footer footer;
    SidebarConfig sidebar;
    LayoutConfig layout;
};
```

Plain. Readable. You can look at this and know exactly what a Site contains. No getters, no setters, no boilerplate. Just the data.

## Namespaces: Organizing Code

Namespaces prevent name collisions and organize code into logical groups:

```cpp
namespace loom {
    struct Post { /* ... */ };
    struct Site { /* ... */ };
}

namespace loom::component {
    struct Ctx { /* ... */ };
    struct Header { /* ... */ };
}

namespace loom::route {
    template<Lit P> inline constexpr get_t<P> get{};
}
```

To use something from a namespace, you qualify it:

```cpp
loom::Site site;
loom::component::Ctx ctx;
```

Or you can bring names into scope:

```cpp
using namespace loom;
Site site;  // no qualification needed
```

Or bring in specific names:

```cpp
using loom::Site;
using loom::Post;
```

Loom uses nested namespaces extensively: `loom::dom` for the HTML DSL, `loom::css` for the CSS DSL, `loom::route` for the routing DSL, `loom::component` for the component system, `loom::theme` for theming. Each namespace is a self-contained vocabulary. When you see `dom::div(class_("container"))`, you know exactly which `div` and which `class_` you're talking about.

## Putting It Together

Here's a tiny program that uses everything we've covered:

```cpp
#include <string>
#include <vector>
#include <iostream>

struct Post {
    std::string title;
    std::string slug;
    bool draft = false;
};

void print_published(const std::vector<Post>& posts) {
    for (const auto& post : posts) {
        if (!post.draft) {
            std::cout << post.title << " (" << post.slug << ")\n";
        }
    }
}

int main() {
    std::vector<Post> posts = {
        {"Values and Types", "values-and-types", false},
        {"Draft Post", "draft", true},
        {"Containers", "containers", false},
    };

    const auto count = posts.size();
    std::cout << "Total: " << count << "\n";

    print_published(posts);
}
```

Values with types. `const` where things shouldn't change. References where copies are wasteful. Structs for grouping data. This is the foundation.

Everything else in C++ — templates, move semantics, concepts, constexpr — is built on top of these ideas. If you understand that C++ is fundamentally about values with known types, and that `const` is how you make promises about those values, the rest of the language will make sense.

Next up: we'll put values into containers and learn to think in collections.
