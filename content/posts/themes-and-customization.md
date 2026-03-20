---
title: 21 Themes, Full Customization, Zero JavaScript Frameworks
date: 2024-02-15
slug: themes-and-customization
tags: feature, themes, architecture
excerpt: 21 built-in themes with dark mode, CSS variable overrides, and full stylesheet replacement. All inlined — no external requests.
---

Loom ships with 21 themes. Each supports automatic dark/light mode based on system preference, with a toggle button for manual override.

## Built-in Themes

**Classic** — clean starting points for any blog:

- **default** — clean, neutral grays, blue accent, system sans-serif
- **serif** — editorial, Georgia/Garamond, warm tones, literary feel
- **mono** — monospace terminal aesthetic, green accent
- **typewriter** — Courier New, old-school ink-on-paper, raw and minimal
- **brutalist** — anti-design, bold borders, red accent, uppercase headings

**Editor Palettes** — themes ported from popular code editors:

- **nord** — arctic color palette, Inter font
- **solarized** — Ethan Schoonover's precision-engineered palette
- **dracula** — the beloved dark-first theme, purple accent
- **gruvbox** — retro groove, warm earthy contrast, orange accent
- **catppuccin** — soothing pastels, purple accent
- **tokyonight** — neon-tinged Tokyo evening palette
- **kanagawa** — inspired by Hokusai's The Great Wave, Charter/Georgia serif

**Warm & Earthy** — cozy, inviting tones:

- **earth** — warm organic tones, Charter/Georgia serif
- **warm** — golden amber palette, Georgia serif, cozy reading feel

**Cool & Blue** — calm, professional blue palettes:

- **cobalt** — deep blue developer theme
- **ocean** — deep blue, calm and professional
- **midnight** — rich dark-first, electric blue, polished

**Soft & Colorful** — gentle pastels and florals:

- **rose** — soft pinks, elegant, magenta accent
- **sakura** — cherry blossom pinks, delicate and refined
- **lavender** — soft purple tones, warm and inviting

**Minimal & Raw** — stripped back, opinionated:

- **hacker** — green on black, monospace, no border-radius, no frills

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
