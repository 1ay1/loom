---
title: Themes
slug: themes
---

## Built-in Themes

Set `theme = name` in `site.conf`. All themes support dark and light mode.

| Theme | Font | Style |
|-------|------|-------|
| `default` | System sans-serif | Clean, neutral grays, blue accent |
| `mono` | Monospace | Terminal aesthetic, green accent |
| `serif` | Georgia / Garamond | Editorial, warm tones, brown accent |
| `nord` | Inter / sans-serif | Arctic palette, muted blues |
| `rose` | System sans-serif | Soft pinks, elegant |
| `cobalt` | System sans-serif | Deep blue developer theme |
| `earth` | Serif | Warm organic tones, olive accent |
| `hacker` | Monospace | Dark-first, green-on-black, no borders |

## Overriding Variables

Tweak a built-in theme without replacing it entirely:

```
theme = nord
theme_accent = #e06c75
theme_dark-accent = #e06c75
theme_font_size = 18px
theme_max_width = 800px
```

Underscores in key names become hyphens in CSS. Prefix with `dark-` for dark-mode-only overrides.

### Available Variables

| Variable | Description |
|----------|-------------|
| `bg` | Background color |
| `text` | Primary text color |
| `muted` | Secondary/muted text |
| `border` | Border and separator color |
| `accent` | Links, highlights, active elements |
| `font` | Font family stack |
| `font-size` | Base font size |
| `max-width` | Content area max width |
| `heading-font` | Heading font (defaults to `inherit`) |
| `code-font` | Code/monospace font |
| `line-height` | Body line height |
| `heading-weight` | Heading font weight |
| `border-radius` | Default border radius |
| `sidebar-width` | Sidebar width |
| `sidebar-gap` | Gap between content and sidebar |
| `nav-gap` | Gap between nav items |
| `header-size` | Site title font size |
| `tag-radius` | Tag badge border radius |
| `tag-bg` | Tag background |
| `tag-text` | Tag text color |
| `link-decoration` | Link text decoration |
| `card-bg` | Card background (cards layout) |
| `card-border` | Card border color |
| `card-radius` | Card border radius |
| `card-padding` | Card padding |
| `accent-hover` | Accent color on hover |
| `header-border-width` | Header bottom border (set `0` to remove) |
| `footer-border-width` | Footer top border |
| `content-width` | Content column max width |

All variables have `dark-` counterparts (e.g., `dark-bg`, `dark-accent`).

## Full CSS Override

Drop a `style.css` in `content/theme/`:

```
content/
  theme/
    style.css    # Replaces the entire default stylesheet
```

This completely replaces the built-in CSS. You're responsible for all layout, typography, and responsive design.

## Custom CSS Additions

For small tweaks without replacing everything:

```
custom_css = .post-full h1 { font-size: 2.2em; } .sidebar { display: none; }
```

This CSS is injected after the theme, so it overrides theme styles.

## Custom Head HTML

Inject anything into `<head>` — Google Fonts, analytics, meta tags:

```
custom_head_html = <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;600;700&display=swap" rel="stylesheet">
```
