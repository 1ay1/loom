---
title: "Ownership, RAII, and Move Semantics — Who Owns What?"
date: 2025-08-29
slug: ownership-and-move-semantics
tags: cpp, ownership, raii, move-semantics, smart-pointers
excerpt: C++ doesn't have a garbage collector. It has something better — a type system that makes ownership explicit.
---

Every resource in a program — memory, file handles, sockets, locks — must be acquired and released. In languages with garbage collection, the runtime handles memory for you (and you handle everything else manually, usually with try-finally). In C++, the language handles *all* resources for you, through a concept called RAII and a mechanism called move semantics.

This is not something to be afraid of. It's something to be grateful for.

## Stack vs. Heap

Every variable lives somewhere in memory. The two places are the stack and the heap:

```cpp
void process() {
    int count = 42;                    // stack
    std::string name = "Loom";         // stack (but string data may be on heap)
    std::vector<Post> posts;           // stack (but elements are on heap)
    auto ptr = std::make_unique<Site>(/*...*/);  // ptr on stack, Site on heap
}
// Everything is destroyed when the function returns
```

Stack allocation is free — it's just a pointer adjustment. Stack deallocation is also free — the stack pointer moves back when the function returns. But the stack is limited in size (typically a few megabytes), and stack objects live only until the function returns.

Heap allocation (`new`, `malloc`, `make_unique`) is slower but flexible — objects can be any size and live as long as you want. The catch: someone has to `delete` them. That's where RAII comes in.

## RAII: Resource Acquisition Is Initialization

RAII is a terrible name for a beautiful idea: **tie a resource's lifetime to a stack object's lifetime.** When the stack object is created, it acquires the resource. When the stack object is destroyed (automatically, when it goes out of scope), it releases the resource.

```cpp
void serve_request() {
    std::string response = build_response();
    // response allocated memory on the heap for its character buffer

    // ... use response ...

}  // response's destructor frees the heap memory — automatically
```

You never call `delete` or `free`. The destructor does it. And destructors run automatically when objects go out of scope. This is not optional, not a convention, not a best practice — it's a language guarantee.

RAII works for any resource:

```cpp
// File handle
{
    std::ofstream file("output.txt");  // opens the file
    file << "data";
}  // file closed automatically

// Mutex lock
{
    std::lock_guard lock(mutex);  // acquires the lock
    shared_data.modify();
}  // lock released automatically
```

Loom's inotify watcher is a perfect RAII example:

```cpp
class InotifyWatcher {
public:
    explicit InotifyWatcher(const std::string& content_dir)
        : content_dir_(content_dir) {}

    ~InotifyWatcher() {
        stop();  // removes watches, closes fd
    }

    InotifyWatcher(const InotifyWatcher&) = delete;
    InotifyWatcher& operator=(const InotifyWatcher&) = delete;
    // ...
private:
    int fd_ = -1;
};
```

The constructor sets up the state. The destructor cleans up. The copy constructor and assignment operator are deleted — you can't accidentally copy a watcher (which would double-close the file descriptor). This is a resource that is born, lives, and dies with its scope.

## unique_ptr: Exclusive Ownership

`std::unique_ptr` is RAII for heap-allocated objects. It owns a pointer and deletes it when destroyed:

```cpp
auto site = std::make_unique<Site>();
site->title = "My Blog";
site->posts.push_back(post);

// site is automatically deleted when unique_ptr goes out of scope
```

The "unique" part means: exactly one `unique_ptr` owns the object. You can't copy it — that would create two owners:

```cpp
auto a = std::make_unique<Site>();
// auto b = a;  // COMPILE ERROR: can't copy unique_ptr
auto b = std::move(a);  // OK: transfers ownership from a to b
// a is now nullptr
```

This is the simplest ownership model: one owner, deterministic destruction. Use `unique_ptr` when you need heap allocation and there's a clear single owner.

## shared_ptr: Shared Ownership

Sometimes multiple parts of the program need to keep an object alive. `std::shared_ptr` uses reference counting:

```cpp
auto cache = std::make_shared<SiteCache>();

auto reader1 = cache;  // refcount = 2
auto reader2 = cache;  // refcount = 3

reader1.reset();  // refcount = 2
reader2.reset();  // refcount = 1
cache.reset();    // refcount = 0 → SiteCache destroyed
```

The object stays alive as long as at least one `shared_ptr` points to it. When the last one dies, the object is destroyed.

Loom uses `shared_ptr<const T>` for its cache pattern. The "const" is crucial — if multiple readers share a pointer, the data must be immutable:

```cpp
template<typename T>
class AtomicCache {
public:
    std::shared_ptr<const T> load() const noexcept {
        return data_.load(std::memory_order_acquire);
    }

    void store(std::shared_ptr<const T> next) noexcept {
        data_.store(std::move(next), std::memory_order_release);
    }

private:
    std::atomic<std::shared_ptr<const T>> data_;
};
```

HTTP request handlers grab a `shared_ptr<const SiteCache>` — a snapshot of the cache at that moment. The hot reloader builds a new cache and atomically swaps it in. Old readers continue using the old cache (kept alive by their shared_ptr). When they finish, the old cache's refcount drops to zero and it's destroyed. No locks needed.

The `shared_ptr<const T>` pattern is how you get safe concurrent reads in C++. The `const` ensures no reader can mutate shared data. The reference count ensures the data lives long enough. The atomic swap ensures readers always see a complete, consistent cache.

## std::move: Transferring Ownership

`std::move` doesn't move anything. It's a cast that says "I'm done with this value — you can take its guts."

```cpp
std::string build_response() {
    std::string body = render_page();  // body owns the string data
    return body;  // compiler moves body into the return value (NRVO)
}
```

When you `std::move` a string, the new string takes ownership of the character buffer and the old string becomes empty. No copying, no allocation — just a pointer swap:

```cpp
std::string a = "a very long string that lives on the heap";
std::string b = std::move(a);
// b now owns the heap buffer
// a is now "" (valid but empty)
```

Loom's route system uses `std::move` extensively to avoid copies:

```cpp
template<Lit P>
struct get_t {
    template<typename H>
    constexpr auto operator()(H h) const {
        return Route<HttpMethod::GET, P, H>{std::move(h)};
    }
};
```

The handler `h` is moved into the Route. No copy of the handler is made. This matters because handlers can be lambdas that capture state — copying them would duplicate that state.

## Move Constructors and Move Assignment

A class supports moving by defining a move constructor and move assignment operator:

```cpp
class Connection {
    std::string read_buf;
    std::shared_ptr<const void> write_ref;
    // ...
};
```

The compiler generates move operations automatically for `Connection` because all its members are movable. `std::string` knows how to move (steal the buffer pointer). `shared_ptr` knows how to move (transfer refcount). The compiler composes these into a whole-object move.

You only need to write your own move operations when managing raw resources:

```cpp
class FileHandle {
public:
    FileHandle(const char* path) : fd_(open(path, O_RDONLY)) {}
    ~FileHandle() { if (fd_ >= 0) close(fd_); }

    FileHandle(FileHandle&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;  // prevent double-close
    }

    FileHandle& operator=(FileHandle&& other) noexcept {
        if (this != &other) {
            if (fd_ >= 0) close(fd_);
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;

private:
    int fd_;
};
```

This is the Rule of Five: if you define any of {destructor, copy constructor, copy assignment, move constructor, move assignment}, you should define all of them. But in practice, you almost never need to, because standard library types handle the hard work.

## explicit Constructors

The `explicit` keyword prevents implicit conversions:

```cpp
template<typename T, typename Tag>
class StrongType {
public:
    explicit StrongType(T value) : value_(std::move(value)) {}
    T get() const { return value_; }
private:
    T value_;
};
```

This is Loom's `StrongType`. The `explicit` prevents this:

```cpp
Slug slug = "hello";  // COMPILE ERROR: no implicit conversion
Slug slug("hello");   // OK: explicit construction
Slug slug = Slug("hello");  // OK
```

Without `explicit`, you could accidentally pass a raw string where a `Slug` is expected, defeating the whole purpose of strong typing. The `explicit` keyword enforces that conversions are intentional.

## The Ownership Hierarchy

Here's how I think about ownership in C++:

1. **By value** (the default): The variable owns the object. It's created when the variable is initialized, destroyed when the variable goes out of scope. Use this for most things.

2. **unique_ptr**: The pointer owns the object on the heap. One owner, one deletion. Use this when you need heap allocation with clear ownership.

3. **shared_ptr<const T>**: Multiple readers share immutable data. Use this for caches, configuration, and anything read by multiple threads.

4. **References (`const T&`)**: Borrowing. No ownership. The caller guarantees the object lives long enough. Use this for function parameters.

5. **string_view, span**: Non-owning views into data owned by someone else. Ultra-lightweight but dangerous if the owner dies first.

Loom follows this hierarchy consistently. Posts, pages, and site config are owned by value in the Site struct. The site cache is shared via `shared_ptr<const T>`. Request parsing uses `string_view` into the connection's read buffer. The blog engine takes `const Site&` — it borrows, doesn't own.

The beauty of this system is that ownership is visible in the type. When you see `shared_ptr<const SiteCache>`, you know it's shared, immutable, reference-counted data. When you see `const Site&`, you know it's borrowed. When you see `std::string`, you know it's owned. The types *are* the documentation.

Next: string_view and zero-copy parsing — where ownership meets performance.
