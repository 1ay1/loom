#include "../../include/loom/render/themes.hpp"

namespace loom
{

static const std::map<std::string, ThemeColors> THEMES = {

    // Clean default — neutral grays, blue accent
    {"default", {
        "#ffffff", "#0f172a", "#64748b", "#e5e7eb", "#2563eb",
        "system-ui,-apple-system,Segoe UI,Roboto,sans-serif", "17px", "720px",
        "#0b0f14", "#e5e7eb", "#94a3b8", "#1f2937", "#60a5fa", {}
    }},

    // Editorial — serif font, warm tones, literary feel
    {"serif", {
        "#faf8f5", "#1a1a1a", "#6b6b6b", "#e0dcd4", "#8b4513",
        "Georgia,Garamond,Times New Roman,serif", "18px", "680px",
        "#1a1712", "#e8e0d4", "#9a9080", "#2e2920", "#d4956a", {}
    }},

    // Mono — terminal/hacker aesthetic
    {"mono", {
        "#f5f5f0", "#1e1e1e", "#666666", "#d4d4d4", "#16a34a",
        "ui-monospace,SFMono-Regular,Menlo,Consolas,monospace", "15px", "760px",
        "#0a0a0a", "#d4d4d4", "#7a7a7a", "#262626", "#4ade80", {}
    }},

    // Nord — arctic color palette
    {"nord", {
        "#eceff4", "#2e3440", "#4c566a", "#d8dee9", "#5e81ac",
        "Inter,system-ui,-apple-system,sans-serif", "16px", "720px",
        "#2e3440", "#d8dee9", "#81a1c1", "#3b4252", "#88c0d0", {}
    }},

    // Rose — soft pinks, elegant
    {"rose", {
        "#fdf2f4", "#1c1017", "#7a6872", "#e8d5db", "#be185d",
        "system-ui,-apple-system,Segoe UI,Roboto,sans-serif", "17px", "720px",
        "#1c1017", "#e8d5db", "#9a8590", "#2d1f27", "#f472b6", {}
    }},

    // Cobalt — deep blue developer theme
    {"cobalt", {
        "#f0f4f8", "#1b2a4a", "#546e8e", "#d0dce8", "#0066cc",
        "system-ui,-apple-system,Segoe UI,Roboto,sans-serif", "17px", "720px",
        "#0d1b2a", "#c8d6e5", "#6b8db5", "#1b2d45", "#48a9fe", {}
    }},

    // Earth — warm, organic tones
    {"earth", {
        "#faf6f0", "#2d2418", "#7a6e5e", "#ddd4c4", "#946b2d",
        "Charter,Georgia,Cambria,serif", "17px", "700px",
        "#1a1610", "#d4c8b0", "#8a7e6e", "#2d2618", "#d4a04a", {}
    }},

    // Hacker — monospace, dark, no gimmicks
    {"hacker", {
        "#f0f0e8", "#1a1a1a", "#5a5a5a", "#c8c8b8", "#2d8a2d",
        "ui-monospace,SFMono-Regular,Menlo,Consolas,monospace", "14px", "800px",
        "#0c0c0c", "#b5b5b5", "#7b7b7b", "#1a1a1a", "#88c070",

        R"CSS(
:root {
  --tag-bg: transparent;
  --tag-text: var(--muted);
  --tag-radius: 0;
}

.tag {
  border: 1px solid var(--muted);
}

.tag:hover {
  border-color: var(--accent);
}

.post-card {
  border-radius: 0;
}

.theme-toggle {
  border-radius: 0;
}

.post-content pre, .page-content pre {
  border: 1px solid var(--muted);
  border-radius: 0;
}

.post-content :not(pre) > code, .page-content :not(pre) > code {
  border-radius: 0;
}

::selection {
  background: #88c070;
  color: #0c0c0c;
}
)CSS"
    }},

    // Solarized — Ethan Schoonover's precision-engineered palette
    {"solarized", {
        "#fdf6e3", "#657b83", "#677171", "#eee8d5", "#268bd2",
        "system-ui,-apple-system,Segoe UI,Roboto,sans-serif", "17px", "720px",
        "#002b36", "#839496", "#819297", "#073642", "#2aa198",

        R"CSS(
::selection {
  background: #268bd2;
  color: #fdf6e3;
}

[data-theme="dark"] ::selection {
  background: #2aa198;
  color: #002b36;
}

.post-content blockquote, .page-content blockquote {
  border-left-color: #cb4b16;
}

[data-theme="dark"] .post-content blockquote,
[data-theme="dark"] .page-content blockquote {
  border-left-color: #cb4b16;
}
)CSS"
    }},

    // Dracula — the mass-beloved dark-first theme
    {"dracula", {
        "#f8f8f2", "#282a36", "#606fa0", "#d6d6e6", "#7c3aed",
        "system-ui,-apple-system,Segoe UI,Roboto,sans-serif", "17px", "720px",
        "#282a36", "#f8f8f2", "#8592b8", "#44475a", "#bd93f9",

        R"CSS(
::selection {
  background: #44475a;
  color: #f8f8f2;
}

.post-content blockquote, .page-content blockquote {
  border-left-color: #ff79c6;
}

:root {
  --tag-bg: color-mix(in srgb, #7c3aed 12%, var(--bg));
  --tag-text: #7c3aed;
}

[data-theme="dark"] {
  --tag-bg: color-mix(in srgb, #bd93f9 20%, #282a36);
  --tag-text: #bd93f9;
}

.post-content a, .page-content a {
  color: #6838b2;
}

[data-theme="dark"] .post-content a,
[data-theme="dark"] .page-content a {
  color: #8be9fd;
}

.post-card:hover {
  border-color: #ff79c6;
}
)CSS"
    }},

    // Gruvbox — retro groove with warm, earthy contrast
    {"gruvbox", {
        "#fbf1c7", "#3c3836", "#766a60", "#d5c4a1", "#d65d0e",
        "system-ui,-apple-system,Segoe UI,Roboto,sans-serif", "17px", "720px",
        "#282828", "#ebdbb2", "#a89984", "#3c3836", "#fe8019",

        R"CSS(
::selection {
  background: #d65d0e;
  color: #fbf1c7;
}

[data-theme="dark"] ::selection {
  background: #fe8019;
  color: #282828;
}

.post-content blockquote, .page-content blockquote {
  border-left-color: #b8bb26;
}

[data-theme="dark"] .post-content blockquote,
[data-theme="dark"] .page-content blockquote {
  border-left-color: #b8bb26;
}

.post-content a, .page-content a {
  color: #458588;
}

[data-theme="dark"] .post-content a,
[data-theme="dark"] .page-content a {
  color: #83a598;
}
)CSS"
    }},

    // Catppuccin — soothing pastel theme for the high-spirited
    {"catppuccin", {
        "#eff1f5", "#4c4f69", "#696b7c", "#ccd0da", "#8839ef",
        "system-ui,-apple-system,Segoe UI,Roboto,sans-serif", "17px", "720px",
        "#1e1e2e", "#cdd6f4", "#a6adc8", "#313244", "#cba6f7",

        R"CSS(
::selection {
  background: #8839ef;
  color: #eff1f5;
}

[data-theme="dark"] ::selection {
  background: #cba6f7;
  color: #1e1e2e;
}

.post-content blockquote, .page-content blockquote {
  border-left-color: #ea76cb;
}

[data-theme="dark"] .post-content blockquote,
[data-theme="dark"] .page-content blockquote {
  border-left-color: #f5c2e7;
}

.post-content a, .page-content a {
  color: #1e66f5;
}

[data-theme="dark"] .post-content a,
[data-theme="dark"] .page-content a {
  color: #89b4fa;
}

:root {
  --tag-bg: color-mix(in srgb, #8839ef 12%, var(--bg));
  --tag-text: #8839ef;
}

[data-theme="dark"] {
  --tag-bg: color-mix(in srgb, #cba6f7 15%, #1e1e2e);
  --tag-text: #cba6f7;
}
)CSS"
    }},

    // Tokyo Night — neon-tinged Tokyo evening palette
    {"tokyonight", {
        "#d5d6db", "#343b58", "#575b70", "#c0c1c8", "#7aa2f7",
        "system-ui,-apple-system,Segoe UI,Roboto,sans-serif", "16px", "720px",
        "#1a1b26", "#a9b1d6", "#7d84a4", "#292e42", "#7aa2f7",

        R"CSS(
::selection {
  background: #7aa2f7;
  color: #1a1b26;
}

.post-content blockquote, .page-content blockquote {
  border-left-color: #ff9e64;
}

[data-theme="dark"] .post-content blockquote,
[data-theme="dark"] .page-content blockquote {
  border-left-color: #ff9e64;
}

.post-content a, .page-content a {
  color: #7aa2f7;
}

[data-theme="dark"] .post-content a,
[data-theme="dark"] .page-content a {
  color: #7dcfff;
}

.post-card:hover {
  border-color: #bb9af7;
}

:root {
  --tag-bg: color-mix(in srgb, #7aa2f7 12%, var(--bg));
  --tag-text: #7aa2f7;
}

[data-theme="dark"] {
  --tag-bg: color-mix(in srgb, #7aa2f7 15%, #1a1b26);
  --tag-text: #7dcfff;
}
)CSS"
    }},

    // Kanagawa — inspired by Katsushika Hokusai's famous painting
    {"kanagawa", {
        "#f2ecbc", "#1F1F28", "#696961", "#d8d3ab", "#957fb8",
        "Charter,Georgia,Cambria,serif", "17px", "700px",
        "#1F1F28", "#DCD7BA", "#8a8983", "#2A2A37", "#957FB8",

        R"CSS(
::selection {
  background: #957fb8;
  color: #DCD7BA;
}

.post-content blockquote, .page-content blockquote {
  border-left-color: #FF5D62;
}

[data-theme="dark"] .post-content blockquote,
[data-theme="dark"] .page-content blockquote {
  border-left-color: #E82424;
}

.post-content a, .page-content a {
  color: #6a9589;
}

[data-theme="dark"] .post-content a,
[data-theme="dark"] .page-content a {
  color: #7FB4CA;
}

:root {
  --tag-bg: color-mix(in srgb, #957fb8 15%, var(--bg));
  --tag-text: #957fb8;
}

[data-theme="dark"] {
  --tag-bg: color-mix(in srgb, #957fb8 20%, #1F1F28);
  --tag-text: #938AA9;
}
)CSS"
    }},

    // Typewriter — old-school ink-on-paper, Courier, raw
    {"typewriter", {
        "#f4f1ea", "#222222", "#6c6c6c", "#d1cdc4", "#222222",
        "Courier New,Courier,monospace", "16px", "640px",
        "#1a1814", "#c8c0b0", "#878279", "#2a2620", "#c8c0b0",

        R"CSS(
::selection {
  background: #222222;
  color: #f4f1ea;
}

[data-theme="dark"] ::selection {
  background: #c8c0b0;
  color: #1a1814;
}

header h1 {
  text-transform: uppercase;
  letter-spacing: 4px;
  font-weight: 400;
}

.post-full h1 {
  text-transform: uppercase;
  letter-spacing: 2px;
}

:root {
  --tag-bg: transparent;
  --tag-text: var(--text);
  --tag-radius: 0;
}

.tag {
  border: 1px solid var(--text);
  text-transform: uppercase;
  font-size: 11px;
  letter-spacing: 1px;
}

.post-card {
  border-radius: 0;
  border-style: dashed;
}

.post-content pre, .page-content pre {
  border-radius: 0;
  border: 1px dashed var(--border);
}

.post-content :not(pre) > code, .page-content :not(pre) > code {
  border-radius: 0;
}

.post-content blockquote, .page-content blockquote {
  border-left: 2px solid var(--text);
  font-style: italic;
}

.theme-toggle {
  border-radius: 0;
}

.post-content img, .page-content img {
  border-radius: 0;
}

.post-content a, .page-content a {
  text-decoration: underline;
  text-decoration-style: dashed;
  text-underline-offset: 3px;
}
)CSS"
    }},

    // Brutalist — raw, functional, anti-design design
    {"brutalist", {
        "#ffffff", "#000000", "#555555", "#000000", "#ff0000",
        "system-ui,-apple-system,Segoe UI,Roboto,sans-serif", "16px", "760px",
        "#000000", "#ffffff", "#aaaaaa", "#ffffff", "#ff4444",

        R"CSS(
::selection {
  background: #ff0000;
  color: #ffffff;
}

[data-theme="dark"] ::selection {
  background: #ff4444;
  color: #000000;
}

header {
  border-bottom-width: 3px;
}

header h1 {
  text-transform: uppercase;
  letter-spacing: 6px;
  font-weight: 900;
}

footer {
  border-top-width: 3px;
}

.post-full h1 {
  text-transform: uppercase;
  letter-spacing: 2px;
  font-weight: 900;
}

.post-card {
  border-radius: 0;
  border-width: 2px;
  border-color: var(--text);
}

.post-card:hover {
  border-color: var(--accent);
  background: var(--accent);
  color: var(--bg);
}

.post-card:hover a {
  color: var(--bg);
}

.post-card:hover .date {
  color: var(--bg);
  opacity: 0.7;
}

:root {
  --tag-bg: var(--text);
  --tag-text: var(--bg);
  --tag-radius: 0;
  --tag-hover-bg: var(--accent);
  --tag-hover-text: var(--bg);
}

.tag {
  text-transform: uppercase;
  font-weight: 700;
  font-size: 11px;
  letter-spacing: 1px;
}

.post-content pre, .page-content pre {
  border-radius: 0;
  border: 2px solid var(--text);
  background: color-mix(in srgb, var(--text) 12%, var(--bg));
}

.post-content :not(pre) > code, .page-content :not(pre) > code {
  border-radius: 0;
  border: 1px solid var(--text);
  background: color-mix(in srgb, var(--text) 8%, var(--bg));
}

.post-content th, .page-content th {
  background: var(--text);
  color: var(--bg);
  font-weight: 700;
  text-transform: uppercase;
  font-size: 12px;
  letter-spacing: 1px;
}

.post-content th, .post-content td,
.page-content th, .page-content td {
  border: 2px solid var(--text);
}

.post-content blockquote, .page-content blockquote {
  border-left: 6px solid var(--accent);
  font-weight: 700;
  color: var(--text);
}

.theme-toggle {
  border-radius: 0;
  border: 2px solid var(--text);
}

.post-content img, .page-content img {
  border-radius: 0;
  border: 2px solid var(--text);
}

.post-content a, .page-content a {
  text-decoration: none;
  background: linear-gradient(var(--accent), var(--accent)) no-repeat 0 100%;
  background-size: 100% 2px;
  padding-bottom: 1px;
}

.post-content a:hover, .page-content a:hover {
  background-size: 100% 100%;
  color: var(--bg);
}

.sidebar {
  border-left: 3px solid var(--text);
}

.sidebar-left .sidebar {
  border-right: 3px solid var(--text);
  border-left: none;
}
)CSS"
    }},

    // Lavender — soft purple tones, warm and inviting
    {"lavender", {
        "#faf8ff", "#2d2640", "#766d8a", "#e6e0f0", "#7c5cbf",
        "system-ui,-apple-system,Segoe UI,Roboto,sans-serif", "17px", "720px",
        "#1a1724", "#e0daea", "#9890a8", "#2a2538", "#a78bfa",

        R"CSS(
:root {
  --tag-bg: color-mix(in srgb, #7c5cbf 8%, var(--bg));
  --tag-text: #7c5cbf;
  --tag-hover-bg: #7c5cbf;
  --tag-hover-text: #ffffff;
  --tag-radius: 14px;
}

[data-theme="dark"] {
  --tag-bg: color-mix(in srgb, #a78bfa 10%, var(--bg));
  --tag-text: #a78bfa;
  --tag-hover-bg: #a78bfa;
  --tag-hover-text: #1a1724;
}

::selection {
  background: #7c5cbf;
  color: #ffffff;
}

[data-theme="dark"] ::selection {
  background: #a78bfa;
  color: #1a1724;
}

.post-card {
  border-radius: 10px;
  transition: border-color 0.2s, box-shadow 0.2s;
}

.post-card:hover {
  box-shadow: 0 2px 12px color-mix(in srgb, #7c5cbf 10%, transparent);
}

.post-listing {
  transition: background 0.15s;
  padding: 14px 10px;
  margin: 0 -10px;
  border-radius: 6px;
}

.post-listing:hover {
  background: color-mix(in srgb, #7c5cbf 4%, var(--bg));
}

.post-content blockquote, .page-content blockquote {
  border-left-color: #7c5cbf;
}

[data-theme="dark"] .post-content blockquote,
[data-theme="dark"] .page-content blockquote {
  border-left-color: #a78bfa;
}

.post-content a, .page-content a {
  color: #6d4dab;
}

[data-theme="dark"] .post-content a,
[data-theme="dark"] .page-content a {
  color: #b49afa;
}
)CSS"
    }},

    // Warm — golden amber palette, cozy reading feel
    {"warm", {
        "#fdf8f0", "#33261a", "#816d5a", "#e8dcc8", "#c47a20",
        "Georgia,Charter,Cambria,Times New Roman,serif", "18px", "700px",
        "#1a1510", "#ddd0b8", "#8a7e68", "#2e2618", "#dda040",

        R"CSS(
:root {
  --tag-bg: color-mix(in srgb, #c47a20 10%, var(--bg));
  --tag-text: #a06418;
  --tag-hover-bg: #c47a20;
  --tag-hover-text: #ffffff;
  --tag-radius: 4px;
  --heading-weight: 600;
}

[data-theme="dark"] {
  --tag-bg: color-mix(in srgb, #dda040 10%, var(--bg));
  --tag-text: #dda040;
  --tag-hover-bg: #dda040;
  --tag-hover-text: #1a1510;
}

::selection {
  background: #c47a20;
  color: #ffffff;
}

[data-theme="dark"] ::selection {
  background: #dda040;
  color: #1a1510;
}

header h1 {
  font-weight: 600;
}

.post-card {
  border-radius: 6px;
  transition: border-color 0.2s, box-shadow 0.2s;
}

.post-card:hover {
  box-shadow: 0 2px 10px color-mix(in srgb, #c47a20 8%, transparent);
}

.post-listing a {
  font-weight: 500;
}

.post-full h1 {
  font-weight: 600;
  line-height: 1.25;
}

.post-content blockquote, .page-content blockquote {
  border-left: 3px solid #c47a20;
  font-style: italic;
}

[data-theme="dark"] .post-content blockquote,
[data-theme="dark"] .page-content blockquote {
  border-left-color: #dda040;
}

.post-content a, .page-content a {
  color: #a06418;
}

[data-theme="dark"] .post-content a,
[data-theme="dark"] .page-content a {
  color: #e8b060;
}
)CSS"
    }},

    // Ocean — deep blue, calm and professional
    {"ocean", {
        "#f4f7fb", "#1a2744", "#5a6e8a", "#d4dce8", "#1a6fb5",
        "system-ui,-apple-system,Segoe UI,Roboto,sans-serif", "17px", "720px",
        "#0c1420", "#c8d4e4", "#6880a0", "#182840", "#4da8e8",

        R"CSS(
:root {
  --tag-bg: color-mix(in srgb, #1a6fb5 8%, var(--bg));
  --tag-text: #1a6fb5;
  --tag-hover-bg: #1a6fb5;
  --tag-hover-text: #ffffff;
  --tag-radius: 4px;
}

[data-theme="dark"] {
  --tag-bg: color-mix(in srgb, #4da8e8 10%, var(--bg));
  --tag-text: #4da8e8;
  --tag-hover-bg: #4da8e8;
  --tag-hover-text: #0c1420;
}

::selection {
  background: #1a6fb5;
  color: #ffffff;
}

[data-theme="dark"] ::selection {
  background: #4da8e8;
  color: #0c1420;
}

header {
  border-bottom-color: color-mix(in srgb, #1a6fb5 15%, var(--border));
}

.post-card {
  border-radius: 8px;
  transition: border-color 0.2s, box-shadow 0.2s;
}

.post-card:hover {
  box-shadow: 0 3px 14px color-mix(in srgb, #1a6fb5 10%, transparent);
}

.post-listing {
  transition: background 0.15s;
  padding: 14px 10px;
  margin: 0 -10px;
  border-radius: 6px;
}

.post-listing:hover {
  background: color-mix(in srgb, #1a6fb5 3%, var(--bg));
}

.post-content blockquote, .page-content blockquote {
  border-left: 3px solid #1a6fb5;
  background: color-mix(in srgb, #1a6fb5 3%, var(--bg));
  border-radius: 0 4px 4px 0;
  padding: 10px 14px;
}

[data-theme="dark"] .post-content blockquote,
[data-theme="dark"] .page-content blockquote {
  border-left-color: #4da8e8;
  background: color-mix(in srgb, #4da8e8 4%, var(--bg));
}

.post-content a, .page-content a {
  color: #1a6fb5;
}

[data-theme="dark"] .post-content a,
[data-theme="dark"] .page-content a {
  color: #68b8f0;
}
)CSS"
    }},

    // Sakura — cherry blossom pinks, delicate and refined
    {"sakura", {
        "#fef9fa", "#3a2030", "#876978", "#ecd8e0", "#d4447a",
        "system-ui,-apple-system,Segoe UI,Roboto,sans-serif", "17px", "720px",
        "#1c1218", "#e0c8d4", "#967887", "#2e1e28", "#f07098",

        R"CSS(
:root {
  --tag-bg: color-mix(in srgb, #d4447a 7%, var(--bg));
  --tag-text: #c03868;
  --tag-hover-bg: #d4447a;
  --tag-hover-text: #ffffff;
  --tag-radius: 14px;
}

[data-theme="dark"] {
  --tag-bg: color-mix(in srgb, #f07098 10%, var(--bg));
  --tag-text: #f07098;
  --tag-hover-bg: #f07098;
  --tag-hover-text: #1c1218;
}

::selection {
  background: #d4447a;
  color: #ffffff;
}

[data-theme="dark"] ::selection {
  background: #f07098;
  color: #1c1218;
}

.post-card {
  border-radius: 10px;
  transition: border-color 0.2s, box-shadow 0.2s;
}

.post-card:hover {
  border-color: color-mix(in srgb, #d4447a 30%, var(--border));
  box-shadow: 0 2px 12px color-mix(in srgb, #d4447a 8%, transparent);
}

.post-listing {
  transition: background 0.15s;
  padding: 14px 10px;
  margin: 0 -10px;
  border-radius: 6px;
}

.post-listing:hover {
  background: color-mix(in srgb, #d4447a 3%, var(--bg));
}

.post-content blockquote, .page-content blockquote {
  border-left-color: #d4447a;
}

[data-theme="dark"] .post-content blockquote,
[data-theme="dark"] .page-content blockquote {
  border-left-color: #f07098;
}

.post-content a, .page-content a {
  color: #c03868;
}

[data-theme="dark"] .post-content a,
[data-theme="dark"] .page-content a {
  color: #f48aaa;
}
)CSS"
    }},

    // Midnight — rich dark-first with electric blue, polished
    {"midnight", {
        "#f0f2f8", "#1e2030", "#5a6080", "#d0d4e0", "#3b82f6",
        "system-ui,-apple-system,Segoe UI,Roboto,sans-serif", "16px", "740px",
        "#0e1018", "#c8cce0", "#757c98", "#1a1e30", "#60a5fa",

        R"CSS(
:root {
  --tag-bg: color-mix(in srgb, #3b82f6 8%, var(--bg));
  --tag-text: #3b82f6;
  --tag-hover-bg: #3b82f6;
  --tag-hover-text: #ffffff;
  --tag-radius: 4px;
}

[data-theme="dark"] {
  --tag-bg: color-mix(in srgb, #60a5fa 10%, var(--bg));
  --tag-text: #60a5fa;
  --tag-hover-bg: #60a5fa;
  --tag-hover-text: #0e1018;
}

::selection {
  background: #3b82f6;
  color: #ffffff;
}

[data-theme="dark"] ::selection {
  background: #60a5fa;
  color: #0e1018;
}

header h1 {
  font-weight: 800;
  letter-spacing: -0.5px;
}

.post-card {
  border-radius: 8px;
  transition: border-color 0.2s, box-shadow 0.2s, transform 0.2s;
}

.post-card:hover {
  border-color: color-mix(in srgb, #3b82f6 30%, var(--border));
  box-shadow: 0 4px 16px color-mix(in srgb, #3b82f6 8%, transparent);
  transform: translateY(-1px);
}

.post-listing {
  transition: background 0.15s;
  padding: 14px 10px;
  margin: 0 -10px;
  border-radius: 6px;
}

.post-listing:hover {
  background: color-mix(in srgb, #3b82f6 3%, var(--bg));
}

.post-full h1 {
  font-weight: 800;
  letter-spacing: -0.5px;
}

.post-content blockquote, .page-content blockquote {
  border-left: 3px solid #3b82f6;
  background: color-mix(in srgb, #3b82f6 3%, var(--bg));
  border-radius: 0 6px 6px 0;
  padding: 10px 14px;
}

[data-theme="dark"] .post-content blockquote,
[data-theme="dark"] .page-content blockquote {
  border-left-color: #60a5fa;
  background: color-mix(in srgb, #60a5fa 4%, var(--bg));
}

.post-content a, .page-content a {
  color: #2563eb;
}

[data-theme="dark"] .post-content a,
[data-theme="dark"] .page-content a {
  color: #78b4fc;
}

.post-content pre, .page-content pre {
  border-radius: 8px;
}

.post-content :not(pre) > code, .page-content :not(pre) > code {
  border-radius: 4px;
}
)CSS"
    }},

};

const std::map<std::string, ThemeColors>& builtin_themes()
{
    return THEMES;
}

std::string theme_to_css(const ThemeColors& t)
{
    std::string css;
    css += ":root{";
    css += "--bg:" + t.bg + ";";
    css += "--text:" + t.text + ";";
    css += "--muted:" + t.muted + ";";
    css += "--border:" + t.border + ";";
    css += "--accent:" + t.accent + ";";
    css += "--font:" + t.font + ";";
    css += "--font-size:" + t.font_size + ";";
    css += "--max-width:" + t.max_width + ";";
    css += "}";
    css += "[data-theme=\"dark\"]{";
    css += "--bg:" + t.dark_bg + ";";
    css += "--text:" + t.dark_text + ";";
    css += "--muted:" + t.dark_muted + ";";
    css += "--border:" + t.dark_border + ";";
    css += "--accent:" + t.dark_accent + ";";
    css += "}";
    if (!t.extra_css.empty())
        css += t.extra_css;
    return css;
}

}
