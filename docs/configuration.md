# Loom Configuration Reference

All configuration lives in `site.conf` inside your content directory. The format is `key = value`, one per line.

## Site Basics

```ini
title = My Blog
description = A blog about things
author = Your Name
```

## Navigation

Comma-separated list of `Label:/path` pairs.

```ini
nav = Home:/, Blog:/tags, About:/about, Projects:/projects
```

These render as links in the site header. Pages are served from `pages/*.md` using the page's slug.

## Footer

```ini
footer_copyright = &copy; 2026 Your Name. All rights reserved.
footer_links = GitHub:https://github.com/you, RSS:/feed.xml, Email:mailto:you@example.com
```

`footer_links` uses the same `Label:URL` format as `nav`.

---

## Layout

These control the structural layout of the site — what's shown, where, and how.

### Header

```ini
header_style = default
```

| Value | Effect |
|---|---|
| `default` | Title + nav on the left, theme toggle on the right |
| `centered` | Title, description, and nav centered |
| `minimal` | Same as default but no bottom border |

```ini
show_description = true
```

Shows the site `description` text below the title in the header. Default: `false`.

```ini
show_theme_toggle = true
```

Shows the dark/light mode toggle button. Default: `true`.

### Post Listings

```ini
post_list_style = list
```

| Value | Effect |
|---|---|
| `list` | One post per line — date, title, tags (default) |
| `cards` | Grid of cards with title, date, and tags |

```ini
show_post_dates = true
show_post_tags = true
```

Toggle visibility of dates and tags in post listings and on individual post pages. Both default to `true`.

### Breadcrumbs

```ini
show_breadcrumbs = true
```

Renders a visible breadcrumb trail at the top of content on post, tag, and series pages. Default: `true`.

| Page type | Trail |
|---|---|
| Post | Home › Post Title |
| Tag page (`/tag/:name`) | Home › Tags › tag-name |
| Tags index (`/tags`) | Home › Tags |
| Series page (`/series/:name`) | Home › Series › series-name |
| Series index (`/series`) | Home › Series |

Set to `false` to hide breadcrumbs site-wide:

```ini
show_breadcrumbs = false
```

### Links

```ini
external_links_new_tab = false
```

When `true`, every external link (`href="https://..."` or `href="http://..."`) across the entire page — content, nav, footer, sidebar — gets `target="_blank" rel="noopener noreferrer"` added automatically. Default: `false`.

For per-link control regardless of this setting, append `^` directly after the closing `)` or `]` in markdown:

```markdown
[Visit site](https://example.com)^          <!-- opens in new tab -->
[Internal link](/about)                     <!-- stays in same tab -->
[ref link][myref]^                          <!-- reference-style, new tab -->
```

The `^` sigil works on all three link forms: inline `[text](url)^`, reference `[text][ref]^`, and shortcut `[text]^`.

### Date Format

```ini
date_format = %Y-%m-%d
```

Any valid `strftime` format string. Used everywhere dates appear (post listings, post pages, sidebar).

| Example | Output |
|---|---|
| `%Y-%m-%d` | 2026-03-14 |
| `%b %d, %Y` | Mar 14, 2026 |
| `%d/%m/%Y` | 14/03/2026 |
| `%B %d, %Y` | March 14, 2026 |

### Sidebar

```ini
sidebar_position = right
```

| Value | Effect |
|---|---|
| `right` | Sidebar on the right (default) |
| `left` | Sidebar on the left |
| `none` | No sidebar (set `sidebar_widgets` to empty) |

The sidebar only renders if `sidebar_widgets` is configured.

```ini
sidebar_widgets = recent_posts, tag_cloud, about
sidebar_recent_count = 5
sidebar_about = A short bio or description that appears in the About widget.
```

Available widgets:

| Widget | Description |
|---|---|
| `recent_posts` | Latest N posts (controlled by `sidebar_recent_count`) |
| `tag_cloud` | All tags as clickable links |
| `about` | Text block from `sidebar_about` |

### Custom Injections

```ini
custom_head_html = <link href="https://fonts.googleapis.com/css2?family=Inter&display=swap" rel="stylesheet">
custom_css = .post-full h1 { font-size: 2.5em; }
```

- `custom_head_html` — injected into `<head>` before the stylesheet. Use for external fonts, analytics scripts, meta tags.
- `custom_css` — injected as a `<style>` block after the theme. Use for targeted overrides.

---

## Theming

Loom uses CSS custom properties for all visual styling. There are three ways to customize the theme, and they can be combined.

### 1. Built-in Themes

```ini
theme = default
```

Loom ships with 6 built-in themes. Each sets colors, fonts, sizing, and structural choices for both light and dark modes.

| Theme | Description |
|---|---|
| `default` | Clean neutral grays, blue accent, system sans-serif |
| `terminal` | Monospace, dark hacker aesthetic, green accent, sharp corners |
| `nord` | Arctic color palette, Inter font, frost blues, soft glow on hover |
| `gruvbox` | Retro groove, warm earthy contrast, orange accent, bordered tags |
| `rose` | Soft pinks, magenta accent, pill-shaped elements, rounded corners |
| `hacker` | CRT phosphor green-on-black, scanlines, blinking cursor, custom components |

Themes define more than colors — they set structural choices like corner rounding (`Corners::Sharp` vs `Round`), tag styles, link decoration, card hover effects, and more.

All code styling is palette-driven. Code blocks derive their background from `var(--text)` and `var(--bg)`, inline code uses `var(--accent)` for text color. No separate light/dark color definitions needed — define your palette and both modes work automatically. See the [Themes](/themes) page for details.

### 2. Theme Variable Overrides

Any CSS variable can be overridden using `theme_*` keys. Underscores in the key name become hyphens in the CSS variable.

```ini
theme_accent = #e63946
theme_font = Inter, system-ui, sans-serif
theme_font_size = 18px
```

This sets `--accent: #e63946`, `--font: Inter, system-ui, sans-serif`, etc.

For dark mode overrides, prefix with `dark_`:

```ini
theme_dark_accent = #ff6b6b
theme_dark_bg = #111111
```

#### All Available Variables

**Colors**

| Variable | Default (light) | Description |
|---|---|---|
| `theme_bg` | `#ffffff` | Page background |
| `theme_text` | `#0f172a` | Main text color |
| `theme_muted` | `#64748b` | Secondary text (dates, meta) |
| `theme_border` | `#e5e7eb` | Borders, dividers, code block background |
| `theme_accent` | `#2563eb` | Links, hover states, active elements |
| `theme_accent_hover` | same as accent | Link/button hover color |
| `theme_dark_bg` | `#0b0f14` | Dark mode background |
| `theme_dark_text` | `#e5e7eb` | Dark mode text |
| `theme_dark_muted` | `#94a3b8` | Dark mode secondary text |
| `theme_dark_border` | `#1f2937` | Dark mode borders |
| `theme_dark_accent` | `#60a5fa` | Dark mode accent |

**Typography**

| Variable | Default | Description |
|---|---|---|
| `theme_font` | `system-ui, -apple-system, ...` | Body font family |
| `theme_heading_font` | `inherit` | Heading font family |
| `theme_code_font` | `ui-monospace, SFMono-Regular, ...` | Code/monospace font |
| `theme_font_size` | `17px` | Base font size |
| `theme_line_height` | `1.7` | Body line height |
| `theme_heading_weight` | `700` | Heading font weight |

**Layout & Spacing**

| Variable | Default | Description |
|---|---|---|
| `theme_max_width` | `720px` | Main content max width |
| `theme_content_width` | same as max-width | Container width |
| `theme_container_padding` | `40px 20px` | Page padding |
| `theme_sidebar_width` | `240px` | Sidebar width |
| `theme_sidebar_gap` | `48px` | Gap between content and sidebar |
| `theme_nav_gap` | `18px` | Gap between nav links |
| `theme_header_size` | `26px` | Site title font size |

**Borders & Shapes**

| Variable | Default | Description |
|---|---|---|
| `theme_border_radius` | `6px` | General border radius (code blocks, buttons) |
| `theme_header_border_width` | `1px` | Header bottom border (`0px` to remove) |
| `theme_footer_border_width` | `1px` | Footer top border (`0px` to remove) |

**Tags**

| Variable | Default | Description |
|---|---|---|
| `theme_tag_radius` | `12px` | Tag pill border radius |
| `theme_tag_bg` | border color | Tag background |
| `theme_tag_text` | muted color | Tag text color |

**Cards** (when `post_list_style = cards`)

| Variable | Default | Description |
|---|---|---|
| `theme_card_bg` | bg color | Card background |
| `theme_card_border` | border color | Card border color |
| `theme_card_radius` | `8px` | Card border radius |
| `theme_card_padding` | `20px` | Card inner padding |

**Links**

| Variable | Default | Description |
|---|---|---|
| `theme_link_decoration` | `underline` | Link text decoration (`none`, `underline`) |

### 3. Full CSS Override

Drop a `theme/style.css` file in your content directory to completely replace the default stylesheet. When this file exists, the built-in CSS is not loaded.

```
content/
  theme/
    style.css    <-- replaces all default styles
```

---

## Content Structure

```
content/
  site.conf
  posts/
    my-first-post.md
    another-post.md
  pages/
    about.md
    projects.md
  theme/
    style.css          (optional)
```

### Post Frontmatter

```
title: My First Post
date: 2026-03-14
slug: my-first-post
tags: go, programming, tutorial

Your markdown content starts here...
```

- `title` — post title (falls back to filename)
- `date` — publish date in `YYYY-MM-DD` format
- `slug` — URL slug (falls back to filename without `.md`)
- `tags` — comma-separated list of tags

### Page Frontmatter

```
title: About
slug: about

Your markdown content starts here...
```

Pages are served at `/:slug` (e.g., `/about`).

---

## Routes

| Route | Description |
|---|---|
| `/` | Post index |
| `/post/:slug` | Single blog post |
| `/tags` | Tag index |
| `/tag/:slug` | Posts filtered by tag |
| `/:slug` | Static page |

---

## Full Example

```ini
# Site
title = Ayush's Blog
description = Writing about code and things
author = Ayush

# Navigation
nav = Home:/, Tags:/tags, About:/about

# Layout
header_style = centered
show_description = true
show_theme_toggle = true
post_list_style = cards
show_post_dates = true
show_post_tags = true
show_breadcrumbs = true
external_links_new_tab = true
date_format = %b %d, %Y
sidebar_position = right

# Sidebar
sidebar_widgets = recent_posts, tag_cloud, about
sidebar_recent_count = 5
sidebar_about = Developer, writer, coffee enthusiast.

# Theme
theme = nord
theme_accent = #88c0d0
theme_font_size = 16px
theme_border_radius = 8px
theme_card_radius = 12px
theme_header_border_width = 0px

# Footer
footer_copyright = &copy; 2026 Ayush
footer_links = GitHub:https://github.com/ayush, RSS:/feed.xml
```
