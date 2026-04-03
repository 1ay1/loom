---
title: "Double Buffering and Frame Lifecycle"
date: 2026-04-01
slug: double-buffering-frame-lifecycle
tags: c++23, double-buffering, frame-lifecycle, synchronized-output, event-driven, claude-code
excerpt: "Two buffers. Swap every frame. Clear, render, replay, diff, emit, swap. The complete lifecycle of a single frame, from event trigger to terminal update, in ~230 microseconds."
---

If you've ever written a game renderer, this chapter will feel familiar. If you haven't, you're about to learn the pattern that makes flicker-free rendering possible — whether you're pushing pixels to a GPU or escape sequences to a terminal.

The core idea: maintain two screen buffers. One represents what's currently on the terminal (`prevScreen`). The other is the frame you're building right now (`currentScreen`). The diff engine compares them. After the diff, they swap roles. Repeat forever.

This is double buffering. It's the same pattern GPUs use: draw to the back buffer while the front buffer is displayed. When the frame is complete, swap.

## Claude Code's Buffer Lifecycle

Here's the render flow I extracted:

```js
class Renderer {
    prevScreen = null;
    currentScreen = null;

    renderFrame(rootNode) {
        let { width, height } = this.getViewport();

        // 1. Reset current screen
        if (!this.currentScreen) {
            this.currentScreen = createScreen(width, height,
                this.stylePool, this.charPool, this.hyperlinkPool);
        } else {
            resetScreen(this.currentScreen, width, height);
        }

        // 2. Build output operations
        let output = new Output(width);
        renderNode(rootNode, output, {
            offsetX: 0, offsetY: 0,
            prevScreen: this.prevScreen,
            inheritedBgColor: null
        });

        // 3. Replay operations onto current screen
        this.replayOperations(output, this.currentScreen);

        // 4. Compute damage rect
        this.currentScreen.damage = output.computeDamage();

        // 5. Diff and emit ANSI
        let ops = this.diff(this.prevScreen, this.currentScreen);
        let ansi = this.serializeOps(ops);

        // 6. Write to terminal
        this.writeToTerminal(ansi);

        // 7. Swap buffers
        [this.prevScreen, this.currentScreen] =
            [this.currentScreen, this.prevScreen];
    }
}
```

Seven steps, executed every frame. Let me walk through each one.

### Step 1: Reset

Clear the current screen buffer. All cells become empty — charId=0, styleId=0, width=0:

```js
function resetScreen(screen, width, height) {
    let total = width * height;
    screen.cells64.fill(0n, 0, total);
    screen.noSelect.fill(0, 0, total);
    screen.width = width;
    screen.height = height;
    screen.damage = undefined;
}
```

`BigInt64Array.fill(0n)` — one call zeros the entire buffer. Internally, this is `memset`. For a 120x40 terminal, that's 38.4KB zeroed in a single call.

Note: `resetScreen` doesn't reallocate unless the buffer is too small. If the terminal shrank, it reuses the existing buffer. If it grew, it allocates a larger one. This avoids allocation on every frame.

### Step 2: Build Output

Walk the render tree, recording operations into the Output builder (Parts 5-6). The blit optimization reads from `prevScreen` — "this subtree hasn't changed, copy it from last frame."

### Step 3: Replay

Execute all recorded operations onto `currentScreen`. Blit operations copy cells from `prevScreen`. Write operations place new cells. Clear operations zero regions. Clip/unclip manage bounding boxes.

### Step 4: Damage

Compute the bounding rectangle of all cells that were written (not blitted). This bounds the diff engine's work in step 5.

### Step 5: Diff

Compare `prevScreen` and `currentScreen` within the damage rect. Emit minimal ANSI escape sequences. This is the entire content of Part 7.

### Step 6: Write

One `write()` syscall with all the ANSI output, wrapped in synchronized output markers.

### Step 7: Swap

The references swap. What was `currentScreen` becomes `prevScreen` for the next frame. What was `prevScreen` becomes the buffer that will be cleared and rewritten.

## Our C++ FrameManager

```cpp
class FrameManager {
public:
    FrameManager(int w, int h, StylePool& styles)
        : styles_(styles)
        , current_(w, h)
        , previous_(w, h)
        , output_()
        , diff_()
    {}

    std::string render_frame(
        std::function<void(OutputBuilder&, const ScreenBuf&)> build) {
        // 1. Reset
        current_.clear();

        // 2-3. Build and replay
        output_.reset();
        build(output_, previous_);
        output_.replay(current_, styles_);

        // 4-5. Diff
        std::string ansi;
        ansi.reserve(current_.w() * current_.h());
        diff_.diff(previous_, current_, styles_, ansi);

        // 6. Swap
        std::swap(previous_, current_);

        return ansi;
    }

private:
    StylePool& styles_;
    ScreenBuf current_;
    ScreenBuf previous_;
    OutputBuilder output_;
    DiffEngine diff_;
};
```

### Buffer Swap Is O(1)

```cpp
std::swap(previous_, current_);
```

This swaps three pointer-sized values: the data pointer, size, and capacity of the underlying `std::vector<SCell>`. No cells are copied. O(1) regardless of buffer size. On a 120x40 terminal, that's swapping 3 values instead of copying 38,400 bytes.

Claude Code's `[prev, next] = [next, prev]` is also O(1) — just swapping references. Same idea, same zero cost.

### Why Full Clear Instead of Selective Clear?

Claude Code clears the entire current buffer every frame. For a 120x40 terminal, that's 38.4KB of writes. An alternative: only clear cells that won't be overwritten.

But selective clearing requires knowing in advance which cells will be written, which requires running the output builder first, which defeats the purpose. The full clear is simpler and fast enough: `memset` of 38.4KB takes ~2 microseconds on modern hardware. The render tree walk takes 50+ microseconds. The clear is noise.

### The First Frame

The first frame has no previous buffer. Claude Code checks for null:

```js
if (!this.prevScreen) {
    ops.push({ type: "clearTerminal" });
}
```

Our approach is simpler — the previous buffer starts as all-empty cells (the `SCell` default constructor fills with spaces and style 0). The first diff sees every non-space cell as "changed" and emits it. No special case needed. The default state naturally handles the first frame.

## Synchronized Output: Atomic Frame Display

Each frame is wrapped in DEC Private Mode 2026 markers:

```cpp
std::string FrameManager::emit_frame(const std::string& ansi) {
    std::string frame;
    frame.reserve(ansi.size() + 16);
    frame += "\033[?2026h";  // begin synchronized update
    frame += ansi;
    frame += "\033[?2026l";  // end synchronized update
    return frame;
}
```

Between the markers, the terminal buffers all incoming output and displays it as a single atomic update. No mid-frame flicker, even if the `write()` syscall gets split by the kernel.

Every modern terminal supports this: kitty, WezTerm, iTerm2, Windows Terminal, Ghostty. Terminals that don't recognize the sequence ignore it harmlessly — the escape is a no-op on unsupported terminals.

### The Flush Pattern

```cpp
void flush_to_terminal(const std::string& frame) {
    ::write(STDOUT_FILENO, frame.data(), frame.size());
}
```

One `write()` call. One chance for interleaving. For large frames (>PIPE_BUF = 4096 bytes), the kernel may split the write internally. The synchronized output markers ensure the terminal still displays atomically — the terminal holds everything until it sees `\033[?2026l`.

## Event-Driven Rendering: No Fixed Frame Rate

Claude Code doesn't run at a fixed frame rate. Rendering is event-driven:

1. React state change -> Ink schedules a render
2. Multiple state changes coalesce into one render
3. Render callback fires -> `renderFrame()`

The frame rate adapts to the rate of change:
- Idle terminal: **0 fps** (no renders, no CPU)
- Spinner animating: **~12 fps** (80ms timer interval)
- Streaming LLM output: **30-60 fps** (as fast as tokens arrive)

Our system follows the same pattern:

```cpp
class Compositor {
    void request_frame() {
        if (frame_pending_) return;  // coalesce
        frame_pending_ = true;
    }

    void process_frame() {
        if (!frame_pending_) return;
        frame_pending_ = false;

        auto ansi = frame_mgr_.render_frame([&](auto& out, auto& prev) {
            presenter_.build_frame(out, prev);
        });

        flush_to_terminal(emit_frame(ansi));
    }
};
```

The `frame_pending_` flag is the coalescing mechanism. If three tokens arrive in rapid succession, `request_frame()` is called three times. `frame_pending_` becomes true on the first call. The second and third calls are no-ops. One frame is rendered, containing all three tokens. This is React's batched updates applied to terminal rendering.

## Memory Reuse: Zero Allocations in Steady State

Both screen buffers are allocated once and reused across all frames. The only reallocation happens on terminal resize:

```cpp
void ScreenBuf::resize(int w, int h) {
    if (w == w_ && h == h_) return;
    w_ = w;
    h_ = h;
    cells_.resize(w * h);  // reallocates only if size increased
    clear();
}
```

`std::vector::resize` only allocates when the new size exceeds capacity. After the first resize to the terminal's dimensions, subsequent frames reuse the same memory.

The output builder's operation vector similarly retains capacity:

```cpp
void OutputBuilder::reset() {
    ops_.clear();  // size -> 0, capacity unchanged
}
```

After a few frames, every data structure has stabilized at its steady-state capacity. Zero allocations per frame. Zero GC pressure (not that we have a GC, but Claude Code does, and this pattern minimizes it there too).

## The Complete Frame Pipeline

```
Event (token arrives, timer fires, key pressed)
    |
    |-- request_frame()  [coalesced]
    |
    v
1. current_.clear()              (memset 38KB,    ~2us)
2. build_frame(output, prev)     (tree walk,      ~50us)
3. output.replay(current)        (cell copies,    ~20us)
4. diff(prev, current) -> ANSI   (damage-bounded, ~100us)
5. "\033[?2026h" + ANSI + "\033[?2026l"  (sync output wrap)
6. write(STDOUT, frame)          (single syscall, ~50us)
7. swap(prev, current)           (pointer swap,   ~0us)
    |
    v
Terminal displays atomic frame update
```

Total frame time for a typical update (spinner + 1 line of streamed text): **~230 microseconds**. The 16ms frame budget gives us **76x headroom**.

We could render at 4000+ fps if the terminal could keep up. In practice, the bottleneck is always the pty write and the terminal emulator's own rendering. Our frame pipeline is so fast it's invisible in the profiler.

---

**Next: [Part 11 — Thread Safety: The Compositor](/post/thread-safety-compositor)**
