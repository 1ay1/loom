# Architecture Rules

## 1. Everything lives in a namespace

All code should be contained within a namespace to avoid naming conflicts.

```cpp
namespace loom
{

}
```

Later, references will become:

```cpp
loom::Post
loom::Server
loom::Router
```

This avoids naming conflicts with other libraries or codebases.

## 2. Header / Source Separation

Headers go into:
```
include/loom/
```

Example future headers:
```cpp
include/loom/post.hpp
include/loom/content_source.hpp
include/loom/blog_engine.hpp
```

Implementations go into:
```
src/
```

Example:
```cpp
src/post.cpp
src/content_source.cpp
src/blog_engine.cpp
```

## 3. One header per concept

Example files:
```cpp
post.hpp
server.hpp
router.hpp
renderer.hpp
```
Avoid huge mega headers.

## 4. Layered architecture

Dependencies should only go downwards.

Correct dependency direction:

```
HTTP Server
   ↓
Router
   ↓
Blog Engine
   ↓
Content Sources
   ↓
Domain Types
```

Never the opposite direction.

For example:

❌ BAD
```cpp
Post includes HTTP headers
```

✔ GOOD
```cpp
Server depends on Post
```

## Rule 5 — Domain first

Before building server code, we design the core domain elements:
```cpp
Post
Slug
Title
Tag
```
Everything else depends on these fundamental types.

## Rule 6 — Value semantics

Prefer:
```cpp
Post
std::vector<Post>
```

Avoid:
```cpp
Post*
shared_ptr<Post>
```
Use raw pointers or smart pointers only when owning resources is necessary.

## Rule 7 — Strong Types

We will avoid using generic types for domain concepts:
```cpp
std::string slug
std::string id
std::string title
```

Instead, we will define specific types for these concepts:
```cpp
Slug
PostId
Title
Tag
```
This approach, known as type-theoretic modeling, helps prevent many common bugs.

## Rule 8 — Compile frequently

Your development loop should be:
```
edit
make
run
```
This facilitates fast iteration.
