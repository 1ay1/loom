#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{

// Terminal — monospace, sharp, modern IDE aesthetic for the C++ enjoyer
inline const ThemeDef terminal = {
    .light = {{"#f4f4f0"}, {"#1a1a2e"}, {"#5a5a6e"}, {"#cdcdc4"}, {"#0969da"}},
    .dark  = {{"#0d1117"}, {"#c9d1d9"}, {"#6e7681"}, {"#21262d"}, {"#58a6ff"}},
    .font  = {"ui-monospace,SFMono-Regular,Menlo,Consolas,Liberation Mono,monospace"},
    .font_size = "14px",
    .max_width = "820px",
    .extra_css = R"CSS(

/* --- reset all curves --- */
.post-card,
.theme-toggle,
.post-content pre, .page-content pre,
.post-content :not(pre) > code, .page-content :not(pre) > code,
.post-content img, .page-content img {
  border-radius: 0;
}

/* --- selection --- */
::selection {
  background: #0969da;
  color: #f4f4f0;
}

[data-theme="dark"] ::selection {
  background: #58a6ff;
  color: #0d1117;
}

/* --- tags look like <angle_brackets> --- */
:root {
  --tag-bg: transparent;
  --tag-text: var(--accent);
  --tag-radius: 0;
  --tag-hover-bg: var(--accent);
  --tag-hover-text: var(--bg);
}

.tag {
  border: 1px solid var(--accent);
  font-size: 12px;
  letter-spacing: 0.5px;
  padding: 2px 6px;
}

.tag::before { content: "<"; opacity: 0.5; }
.tag::after  { content: ">"; opacity: 0.5; }

/* --- header: namespace style --- */
header h1 {
  letter-spacing: 3px;
  font-weight: 700;
  text-transform: lowercase;
}

header h1::after {
  content: "::";
  opacity: 0.3;
  margin-left: 2px;
}

/* --- post cards: left-border accent, sharp --- */
.post-card {
  border-left: 3px solid var(--accent);
  transition: border-color 0.15s, background 0.15s;
}

.post-card:hover {
  border-left-color: var(--text);
  background: color-mix(in srgb, var(--accent) 4%, var(--bg));
}

/* --- post title --- */
.post-full h1 {
  font-weight: 700;
  letter-spacing: -0.3px;
  line-height: 1.3;
}

/* --- listings --- */
.post-listing {
  transition: background 0.12s;
  padding: 12px 10px;
  margin: 0 -10px;
}

.post-listing:hover {
  background: color-mix(in srgb, var(--accent) 4%, var(--bg));
}

.post-listing .date {
  font-variant-numeric: tabular-nums;
}

/* --- links: dotted underline like man pages --- */
.post-content a, .page-content a {
  color: var(--accent);
  text-decoration: underline;
  text-decoration-style: dotted;
  text-underline-offset: 3px;
}

.post-content a:hover, .page-content a:hover {
  text-decoration-style: solid;
}

/* --- code blocks: terminal window feel --- */
.post-content pre, .page-content pre {
  border: 1px solid var(--border);
  border-left: 3px solid var(--accent);
  background: color-mix(in srgb, var(--text) 4%, var(--bg));
}

.post-content :not(pre) > code, .page-content :not(pre) > code {
  border: 1px solid var(--border);
  background: color-mix(in srgb, var(--text) 5%, var(--bg));
  padding: 1px 5px;
  font-size: 0.9em;
}

/* --- blockquotes: styled like // comments --- */
.post-content blockquote, .page-content blockquote {
  border-left: 2px solid var(--muted);
  font-style: italic;
  opacity: 0.85;
}

.post-content blockquote::before {
  content: "// ";
  font-style: normal;
  opacity: 0.4;
  font-size: 0.85em;
}

/* --- tables: tight grid --- */
.post-content th, .page-content th {
  background: color-mix(in srgb, var(--text) 8%, var(--bg));
  text-transform: uppercase;
  font-size: 11px;
  letter-spacing: 1px;
  font-weight: 600;
}

.post-content th, .post-content td,
.page-content th, .page-content td {
  border: 1px solid var(--border);
}

/* --- theme toggle: square --- */
.theme-toggle {
  border: 1px solid var(--border);
}

/* --- cursor blink on footer (subtle) --- */
footer::after {
  content: "_";
  animation: blink 1s step-end infinite;
  opacity: 0.4;
  margin-left: 4px;
}

@keyframes blink {
  50% { opacity: 0; }
}

)CSS",
};

} // namespace loom::theme::builtin
