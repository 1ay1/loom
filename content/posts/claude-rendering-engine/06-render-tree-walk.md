---
title: "The Render Tree Walk — From Layout to Cells"
date: 2026-03-13
slug: render-tree-walk-layout-to-cells
tags: c++23, render-tree, yoga-layout, blit, dirty-flag, component-tree, claude-code
excerpt: "The renderNode function is the most complex piece in the pipeline. It walks the component tree, decides what to re-render and what to skip, and orchestrates the entire frame. I extracted it from minified JavaScript, and the architecture is beautiful."
---

We've built the cell grid. We've built the interning pools. We've built the output builder. Now we need the thing that actually *drives* the rendering — the function that walks the component tree, reads layout coordinates, and decides what goes where.

In Claude Code's architecture, Yoga (the layout engine from React Native) computes where every component goes — x, y, width, height. The render tree walk reads those coordinates and writes cell data into the Output builder. It's the bridge between "here's what the UI looks like" and "here are the cells that represent it."

This is the `renderNode` function — `LJT` in the minified binary. It took me the longest to reconstruct because it handles every node type, every edge case, and the blit optimization that makes the whole system fast.

## The Full renderNode, Reconstructed

Here's what I pieced together from the decompiled binary:

```js
function renderNode(node, output, context) {
    let { offsetX, offsetY, prevScreen, inheritedBgColor } = context;
    let { yogaNode } = node;
    if (!yogaNode) return;

    // === HIDDEN NODES ===
    if (yogaNode.getDisplay() === Display.None) {
        if (node.dirty) {
            let prev = layoutCache.get(node);
            if (prev) output.clear(prev);
            layoutCache.delete(node);
        }
        return;
    }

    // === COMPUTE LAYOUT ===
    let x = offsetX + yogaNode.getComputedLeft();
    let y = offsetY + yogaNode.getComputedTop();
    let w = yogaNode.getComputedWidth();
    let h = yogaNode.getComputedHeight();
    let prevLayout = layoutCache.get(node);

    // === BLIT OPTIMIZATION ===
    if (!node.dirty
        && node.pendingScrollDelta === undefined
        && prevLayout
        && prevLayout.x === x && prevLayout.y === y
        && prevLayout.width === w && prevLayout.height === h
        && prevScreen) {
        output.blit(prevScreen, Math.floor(x), Math.floor(y),
                     Math.floor(w), Math.floor(h));
        blitAbsoluteChildren(node, output, prevScreen, x, y, w, h);
        return;
    }

    // === MOVED/RESIZED: CLEAR OLD REGION ===
    if (prevLayout) {
        if (prevLayout.x !== x || prevLayout.y !== y
            || prevLayout.width !== w || prevLayout.height !== h) {
            output.clear(prevLayout);
        }
    }

    // === BACKGROUND FILL ===
    let bgColor = node.style?.backgroundColor ?? inheritedBgColor;
    if (bgColor && bgColor !== inheritedBgColor) {
        let bgStyle = stylePool.intern(makeBgTokens(bgColor));
        for (let row = 0; row < h; row++) {
            output.write(x, y + row, spaceFill(w, bgStyle));
        }
    }

    // === TEXT NODES ===
    if (node.nodeName === "ink-text") {
        renderTextNode(node, output, x, y, w, h, bgColor);
    }

    // === RAW ANSI PASSTHROUGH ===
    if (node.nodeName === "ink-raw-ansi") {
        output.write(x, y, node.attributes.rawText);
    }

    // === OVERFLOW HANDLING ===
    let hasOverflow = yogaNode.getOverflow() === Overflow.Scroll
                   || yogaNode.getOverflow() === Overflow.Hidden;
    if (hasOverflow) {
        output.clip({ x, y, width: w, height: h });
    }

    // === SCROLL DELTA ===
    if (node.pendingScrollDelta !== undefined && hasOverflow) {
        output.shift(Math.floor(y), Math.floor(y + h),
                     node.pendingScrollDelta);
        node.pendingScrollDelta = undefined;
    }

    // === RECURSE INTO CHILDREN ===
    for (let child of node.childNodes) {
        if (child.yogaNode?.getPositionType() !== PositionType.Absolute) {
            renderNode(child, output, {
                offsetX: x,
                offsetY: y - (node.scrollTop ?? 0),
                prevScreen,
                inheritedBgColor: bgColor
            });
        }
    }

    // Absolute children rendered after (on top)
    for (let child of node.childNodes) {
        if (child.yogaNode?.getPositionType() === PositionType.Absolute) {
            renderNode(child, output, {
                offsetX: x,
                offsetY: y,
                prevScreen,
                inheritedBgColor: bgColor
            });
        }
    }

    if (hasOverflow) output.unclip();

    // === CACHE LAYOUT FOR NEXT FRAME ===
    layoutCache.set(node, { x, y, width: w, height: h });
    node.dirty = false;
}
```

That's a lot of code. But every node falls into one of three paths, and once you see the pattern, the complexity dissolves.

## The Three Paths

### Path 1: Blit (Skip Everything)

Clean node. Hasn't moved. Same size. Previous frame exists.

**Cost:** one rectangle copy. The entire subtree is *never visited*. Children don't get walked. Text doesn't get re-rendered. Styles don't get re-interned. Nothing happens except a `memcpy` of the rectangle from the previous screen buffer.

This is the fast path, and it's the path most nodes take most of the time.

### Path 2: Text (Render Content)

Ink text node. Compute styled spans, handle word wrap, write to output.

This is where actual content enters the pipeline. The text renderer has already done syntax highlighting and style computation — the tree walk just places the pre-rendered characters at the right coordinates.

### Path 3: Container (Recurse)

Has children. Compute background, handle overflow/clip, recurse into child nodes.

The container itself may write nothing visible — it's just a layout boundary that affects positioning and clipping for its children.

## The Dirty Flag: How 99% of Work Gets Skipped

Each node has a `dirty` boolean. When a component's state changes (React re-render in Claude Code's case), the framework marks that node and its ancestors dirty. Only dirty subtrees need re-rendering.

Visualize a typical frame during streaming output:

```
Root (dirty)
|-- Header (clean)        -> BLIT entire header from prevScreen
|-- Content (dirty)
|   |-- Message 1 (clean) -> BLIT
|   |-- Message 2 (clean) -> BLIT
|   |-- Message 3 (dirty) -> re-render text spans
|-- StatusBar (dirty)      -> re-render spinner + status
```

One message is streaming (dirty). The spinner is animating (dirty). Everything else — every previous message, every tool call card, every box-drawn border — is blitted from the previous frame at memory-copy speed.

The render tree walk visits only the dirty nodes. In a conversation with 50 messages, that's 2 out of 50+. The tree walk doesn't even *look at* the other 48 nodes.

## Our C++ Adaptation

We don't have React or Yoga. Our component model is simpler — the presenter knows what regions of the screen contain what content. But we apply the same pattern:

```cpp
struct Region {
    int x, y, w, h;
    bool dirty = true;

    bool matches(int nx, int ny, int nw, int nh) const {
        return x == nx && y == ny && w == nw && h == nh;
    }
};

class Presenter {
    Region status_region_;      // top status bar
    Region content_region_;     // main conversation area
    Region input_region_;       // bottom input line
    Region spinner_region_;     // spinner overlay

    ScreenBuf current_;
    ScreenBuf previous_;
    OutputBuilder output_;

    void render_frame() {
        output_.reset();

        // Status bar: only re-render if status changed
        if (status_region_.dirty) {
            render_status(output_);
            status_region_.dirty = false;
        } else {
            output_.blit(previous_, status_region_.x, status_region_.y,
                         status_region_.w, status_region_.h);
        }

        // Content: only re-render dirty portion
        if (content_region_.dirty) {
            render_content(output_);
            content_region_.dirty = false;
        } else {
            output_.blit(previous_, content_region_.x, content_region_.y,
                         content_region_.w, content_region_.h);
        }

        // Input line: re-render on every keystroke
        render_input(output_);

        // Replay onto current buffer
        output_.replay(current_, style_pool_);

        // Diff and emit
        diff_and_emit(previous_, current_);

        // Swap buffers
        std::swap(previous_, current_);
    }
};
```

### The Scroll Offset Pattern

When the user scrolls, we don't re-render all content. We shift existing cells and render only the newly visible rows:

```cpp
void Presenter::scroll_content(int delta) {
    output_.clip(content_region_.x, content_region_.y,
                 content_region_.w, content_region_.h);
    output_.shift(content_region_.y,
                  content_region_.y + content_region_.h, delta);

    // Only render the newly visible rows
    if (delta > 0) {
        // Scrolled up: new rows appear at bottom
        int first_new = content_region_.y + content_region_.h - delta;
        render_content_rows(output_, first_new, delta);
    } else {
        // Scrolled down: new rows appear at top
        render_content_rows(output_, content_region_.y, -delta);
    }

    output_.unclip();
}
```

This is exactly what Claude Code does with `pendingScrollDelta`. The shift moves existing cells. Only the delta rows are re-rendered. For a 40-row content area scrolling by 1 line, this renders 1 row instead of 40. A 97.5% reduction in work.

## Text Rendering: Styled Spans to Cells

The text rendering path converts styled characters into cell writes. Our equivalent takes `RenderedLine` (from the markdown renderer) and places it:

```cpp
void write_rendered_line(OutputBuilder& out, int x, int y,
                         const RenderedLine& line, int max_width) {
    int col = x;
    for (const auto& sc : line.chars) {
        if (col - x >= max_width) break;
        out.write(col, y, std::span(&sc, 1));
        col += std::max(sc.width, (uint8_t)1);
    }
}
```

The markdown renderer has already done the hard work — parsing, syntax highlighting, computing styles. The render tree walk just places pre-rendered lines at the right coordinates. Separation of concerns at work.

## Background Inheritance

Claude Code passes `inheritedBgColor` down the tree. If a parent has a background color, children inherit it unless they override. This matters for containers like tool cards and diff blocks where the background extends beyond the text content.

In our model, this is more explicit:

```cpp
void render_tool_card(OutputBuilder& out, const ToolCard& card,
                      int x, int y, int w) {
    // Fill entire card area with card background
    StyleId card_bg = style_pool_.intern(
        PackedStyle{}.with_bg(palette::surface0));

    for (int r = 0; r < card.height(); ++r)
        out.fill(x, y + r, w, U' ', card_bg);

    // Write content on top -- it inherits bg from the fill
    for (int i = 0; i < card.lines(); ++i)
        write_rendered_line(out, x + 1, y + i, card.line(i), w - 2);
}
```

## Layout Cache and Ghost Prevention

Claude Code caches the layout rect `{x, y, width, height}` for each node between frames. This enables two critical behaviors:

1. **Blit comparison**: Is the node at the same position and size as last frame? If yes and clean, blit.

2. **Ghost prevention**: If the node moved, clear the *old* position before rendering at the new position. Without the cache, moved nodes leave ghost images — stale characters at the old position that never get erased.

Our equivalent:

```cpp
void Presenter::update_layout(int new_width, int new_height) {
    Region old_content = content_region_;

    // Recompute layout
    status_region_  = {0, 0, new_width, 1};
    content_region_ = {0, 1, new_width, new_height - 2};
    input_region_   = {0, new_height - 1, new_width, 1};

    // If content region moved or resized, clear old area
    if (!old_content.matches(content_region_.x, content_region_.y,
                              content_region_.w, content_region_.h)) {
        output_.clear(old_content.x, old_content.y,
                      old_content.w, old_content.h);
        content_region_.dirty = true;
    }
}
```

## Absolute Positioning: Overlays

Claude Code renders absolute-positioned children *after* normal children so they stack on top. In our TUI, overlays like the spinner, permission prompts, and autocomplete dropdowns serve the same purpose:

```cpp
void Presenter::render_frame() {
    // Normal flow
    render_status(output_);
    render_content(output_);
    render_input(output_);

    // Overlays (absolute positioning equivalent)
    if (spinner_visible_)
        render_spinner(output_);    // on top of status
    if (permission_prompt_)
        render_permission(output_); // modal overlay
}
```

Render order = z-order. Last rendered = on top. Simple as paint.

## The Key Insight

The render tree walk's real job isn't rendering. It's *deciding what not to render.*

```
For each node:
    1. Hidden?         -> clear old rect, skip
    2. Clean + unmoved -> blit from previous frame, skip subtree
    3. Moved/resized?  -> clear old rect
    4. Has background? -> fill rect with bg color
    5. Text node?      -> render styled text spans
    6. Has overflow?   -> push clip rect
    7. Has scroll?     -> shift cells
    8. Recurse children (normal first, absolute after)
    9. Pop clip
    10. Cache layout, clear dirty flag
```

Step 2 is the whole game. **Most nodes are clean most of the time.** A frame that streams one token of LLM output touches one line of one message. Everything else — the header, the other messages, the tool call history, the box decorations — all blitted. The render tree walk's job is to identify the 1% that changed and skip the 99% that didn't.

This is retained-mode rendering applied to a terminal. The screen is a persistent canvas. Most of it doesn't change between frames. The walk finds the changes, the output builder records them, the replay engine applies them, and the diff engine emits the minimal ANSI to make it real.

---

**Next: [Part 7 — The Diff Engine](/post/diff-engine-only-paint-what-changed)**
