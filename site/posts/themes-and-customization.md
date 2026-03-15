---
title: 8 Themes, Full Customization, Zero JavaScript Frameworks
date: 2024-02-15
slug: themes-and-customization
tags: feature, themes, architecture
excerpt: Built-in themes with dark mode, CSS variable overrides, and full stylesheet replacement. All inlined — no external requests.
---

Loom ships with 8 themes. Each supports automatic dark/light mode based on system preference, with a toggle button for manual override.

## Built-in Themes

- **default** — clean, neutral, system sans-serif
- **mono** — monospace terminal aesthetic
- **serif** — editorial, Georgia/Garamond, warm tones
- **nord** — arctic color palette
- **rose** — soft pinks
- **cobalt** — deep blue
- **earth** — warm organic tones, serif
- **hacker** — green on black, monospace, no borders

Set with `theme = name` in `site.conf`. Hot reload picks up the change instantly.

## Variable Overrides

Don't like the accent color? Override just that:

```
theme = nord
theme_accent = #e06c75
theme_dark-accent = #e06c75
```

Any CSS custom property can be overridden. Underscores become hyphens. `dark-` prefix targets dark mode only. No need to write a full stylesheet.

## Full CSS Override

Drop `style.css` in `content/theme/` to replace the entire stylesheet. Loom detects the file and uses it instead of the built-in CSS.

## How It Works

All CSS is inlined in the `<head>`. No external stylesheet requests. The dark mode toggle uses a small inline script that runs before paint — no FOUC (Flash of Unstyled Content).

Theme detection priority:

1. `localStorage` saved preference
2. `prefers-color-scheme` media query
3. Light mode default

The theme toggle, dark mode JS, and all CSS ship in a single HTML response. Zero external requests.
