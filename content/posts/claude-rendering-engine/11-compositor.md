---
title: "Thread Safety — The Compositor"
date: 2026-03-23
slug: thread-safety-compositor
tags: c++23, compositor, mutex, thread-safety, signal-handling, self-pipe, claude-code
excerpt: "The spinner thread and the main thread both want stdout. They can't have it. The Compositor is the bouncer: one lock, one writer, zero corruption. Plus the self-pipe trick, mutable regions, and why the mutex must cover the write() syscall."
---

Remember Part 1? Two threads, one stdout, terminal corruption? We've spent ten chapters building a beautiful rendering engine. Now we need to make sure multiple threads don't destroy it.

Claude Code has it easy here. It runs in a single-threaded Node.js event loop (compiled with Bun). JavaScript's run-to-completion semantics guarantee that a render callback runs atomically. The spinner uses `setInterval`, which fires between event loop ticks, never interrupting a render in progress. No data races. No mutexes. No problems.

We don't have that luxury.

## Our Concurrency Model

We have real threads:

```
Thread 1 (main):     agent_loop -> process tokens -> update presenter
Thread 2 (spinner):  while(running) { tick(); sleep(80ms); }
Thread 3 (input):    while(running) { read_key(); process(); }
Signal handler:      SIGWINCH -> update dimensions
```

All of these produce terminal output. None of them can write to stdout directly. The Compositor is the synchronization point — the single owner of the terminal.

## The Compositor Pattern

```cpp
class Compositor {
public:
    explicit Compositor(int fd = STDOUT_FILENO);

    // Thread-safe write interface
    [[nodiscard]] auto lock() -> std::unique_lock<std::mutex>;

    // Status bar: cell-based diffing (spinner thread)
    void set_status(std::span<const StyledChar> cells);

    // Append mode: streaming content (main thread)
    void write(std::string_view text);
    void flush();

    // Mutable region: line-based diffing (any thread)
    void begin_mutable(int line_count);
    void set_mutable_line(int idx, const RenderedLine& line);
    void end_mutable();

    // Screen management
    void resize(int w, int h);

private:
    int fd_;
    std::mutex mtx_;

    // Status bar state
    std::vector<StyledChar> status_cells_;
    std::vector<StyledChar> prev_status_;

    // Mutable region state
    std::vector<RenderedLine> mutable_lines_;
    std::vector<RenderedLine> prev_mutable_;
    int mutable_start_row_ = 0;

    // Output buffer
    std::string buf_;

    void emit(std::string_view s);
    void flush_buf();
    void diff_status();
    void diff_mutable();
};
```

Three write modes for three different update patterns. Each one uses the diff strategy that makes sense for its use case.

### The Lock Guard

Every public method acquires the mutex. But sometimes you need multiple operations to be atomic:

```cpp
{
    auto guard = compositor.lock();
    compositor.set_status(status_cells);
    compositor.set_mutable_line(0, spinner_line);
    compositor.end_mutable();
}  // guard destroyed -> mutex released -> output flushed
```

Without the guard, another thread could interleave writes between `set_status` and `set_mutable_line`, producing a frame where the status bar is updated but the mutable region isn't — a visually inconsistent state that lasts one frame but is perceptible as flicker.

### Status Bar: Cell-Level Diff

The spinner changes 1-3 cells every 80ms. We don't redraw the entire status bar — we diff it at the cell level:

```cpp
void Compositor::diff_status() {
    int width = std::min(status_cells_.size(), prev_status_.size());

    for (int i = 0; i < width; ++i) {
        if (status_cells_[i] == prev_status_[i]) continue;

        buf_ += std::format("\033[{};{}H", status_row_ + 1, i + 1);
        const auto& sc = status_cells_[i];
        buf_ += style_pool_.ansi(style_pool_.intern(sc.style));
        encode_one(sc.ch, buf_);
    }

    prev_status_ = status_cells_;
}
```

Same algorithm as the full-screen diff engine from Part 7, specialized for one row. The spinner changes maybe 3 cells. That's ~20 bytes of ANSI output. The rest of the status bar (model name, token count, latency) doesn't change between ticks, so it's skipped.

### Mutable Region: Line-Level Diff

For multi-line regions that update in place — tool output, permission prompts, progress indicators:

```cpp
void Compositor::diff_mutable() {
    for (int i = 0; i < mutable_lines_.size(); ++i) {
        if (i < prev_mutable_.size()
            && mutable_lines_[i] == prev_mutable_[i])
            continue;

        buf_ += std::format("\033[{};1H\033[2K",
                            mutable_start_row_ + i + 1);

        for (const auto& sc : mutable_lines_[i].chars) {
            buf_ += style_pool_.ansi(style_pool_.intern(sc.style));
            encode_one(sc.ch, buf_);
        }
    }

    // Clear extra lines if region shrunk
    for (int i = mutable_lines_.size(); i < prev_mutable_.size(); ++i) {
        buf_ += std::format("\033[{};1H\033[2K",
                            mutable_start_row_ + i + 1);
    }

    prev_mutable_ = mutable_lines_;
}
```

Line-level diffing is coarser than cell-level but simpler. For regions where entire lines change at once (code blocks, formatted output), comparing whole lines is sufficient and the per-cell overhead isn't worth it.

## The Spinner Thread

The spinner is the most frequent concurrent writer:

```cpp
void Presenter::spinner_loop() {
    const char* frames[] = {"⠋","⠙","⠹","⠸","⠼","⠴","⠦","⠧","⠇","⠏"};
    int frame = 0;

    while (spinner_running_) {
        {
            auto guard = comp_.lock();
            auto cells = build_status_cells(frames[frame]);
            comp_.set_status(cells);
        }  // guard released -> status diff + flush

        frame = (frame + 1) % 10;
        std::this_thread::sleep_for(80ms);
    }
}
```

Lock, update, unlock. The critical section contains the diff and flush — the terminal write happens while holding the lock.

### Why the Lock Must Cover the Flush

This is the single most common concurrency mistake in terminal UI code:

```cpp
// WRONG: race between unlock and write
{
    auto guard = comp_.lock();
    comp_.set_status(cells);
}
// Thread B could write to stdout HERE
comp_.flush();  // interleaved!
```

The lock must extend from the state mutation through the `write()` syscall:

```cpp
// RIGHT: atomic from state change through terminal write
{
    auto guard = comp_.lock();
    comp_.set_status(cells);
    // set_status internally: diff -> build ANSI -> write(fd, ...)
}
```

The mutex protects **stdout, not just the data structures**. Any thread that produces terminal output must hold the lock until the bytes have been written. If you release the lock between building the ANSI and writing it, another thread can slip its output in between, and you're back to the interleaving problem from Part 1.

## Priority: Spinner vs. Main Thread

The spinner is cosmetic. The main thread processes LLM output that the user is waiting for. If the spinner holds the lock while a token arrives, the user sees latency.

We minimize this by keeping the spinner's critical section tiny:

```cpp
// Spinner: holds lock for ~50us
// (diff 3 cells + write ~20 bytes)
{
    auto guard = comp_.lock();
    comp_.set_status(cells);
}

// Main thread: holds lock for ~200us
// (append text + flush)
{
    auto guard = comp_.lock();
    comp_.write(token);
    comp_.flush();
}
```

Worst case: the main thread arrives just after the spinner acquires the lock. It waits 50 microseconds. That's 0.05ms. Completely imperceptible. The user would need superhuman perception to notice a 50-microsecond delay on a streaming token that's already traveling over the network with multi-millisecond latency.

## Resize Handling: The Self-Pipe Trick

`SIGWINCH` fires when the terminal resizes. Signal handlers have brutal constraints — you can't acquire mutexes, allocate memory, or call most library functions. The only safe operations are setting `volatile sig_atomic_t` and writing to a pipe.

We use the self-pipe trick:

```cpp
static int resize_pipe[2];

void sigwinch_handler(int) {
    // Signal-safe: write one byte to wake the main thread
    char c = 1;
    ::write(resize_pipe[1], &c, 1);
}

// On the main thread's event loop:
void check_resize() {
    char c;
    if (::read(resize_pipe[0], &c, 1) == 1) {
        struct winsize ws;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
        auto guard = comp_.lock();
        comp_.resize(ws.ws_col, ws.ws_row);
    }
}
```

The signal handler writes a byte to a pipe. The main thread reads from the pipe and performs the actual resize under the compositor's lock. The signal handler does no locking, no allocation, no undefined behavior. The complexity is handled on the main thread where it's safe.

## The Three Write Modes

| Mode | Use case | Diff strategy | Typical output |
|------|----------|--------------|----------------|
| Status bar | Spinner, token count, model name | Cell-level diff | ~20 bytes/tick |
| Append | Streaming LLM text, tool output | No diff (append-only) | ~50 bytes/token |
| Mutable | Permission prompts, progress bars | Line-level diff | ~200 bytes/update |

This is simpler than the full double-buffered architecture from Part 10. We don't use the output builder pattern or damage rectangles for the compositor — instead, we provide three direct write modes with their own specialized diff strategies.

This is a pragmatic choice. The full output builder / double buffer / damage rect system from Claude Code is more powerful (cell-level diffing for everything, blit optimization across all regions) but also more complex. Our three-mode compositor handles the common cases efficiently. A future version could upgrade to the full architecture while keeping the same thread-safe API.

## The Single Writer Guarantee

The compositor is the **only** code that calls `write(STDOUT_FILENO, ...)`. No other thread, function, or signal handler writes to the terminal. This is enforced by convention and architecture:

```cpp
class Presenter {
    Compositor& comp_;  // the ONLY way to produce output

    // No reference to stdout
    // No direct write calls
    // All output goes through comp_.write(), comp_.set_status(), etc.
};
```

If a future developer adds a `printf` or `std::cout <<` somewhere, the terminal will corrupt. The `static_assert` for this is code review. The compiler can't prevent someone from calling `write(1, ...)`, but the architecture makes the correct path obvious and the incorrect path require deliberate effort.

## The Big Picture

```
Main Thread          Spinner Thread        Signal Handler
    |                    |                     |
    |  lock()            |  lock()             |
    +------> Compositor <+------> Compositor   |
    |  write(token)      |  set_status(cells)  |
    |  flush()           |  [auto diff+flush]  |  write(pipe, 1)
    |  unlock()          |  unlock()           |      |
    |                    |                     |      v
    |                    |                   Main thread reads pipe
    |                    |                   lock() + resize() + unlock()
    v                    v
               +----------------+
               |    stdout      |  <-- single writer, always
               |    (fd 1)      |
               +----------------+
                      |
                      v
               +----------------+
               |    Terminal    |
               +----------------+
```

The compositor serializes concurrent producers into sequential terminal output. The mutex ensures atomicity. The diff strategies ensure minimal bytes. The synchronized output markers ensure flicker-free display.

Every byte that reaches the terminal went through exactly one code path. Every frame is consistent. Every update is atomic. The terminal corruption from Part 1 is structurally impossible.

---

**Next: [Part 12 — Putting It All Together](/post/full-pipeline-putting-it-together)**
