#pragma once

#include "../theme_def.hpp"

namespace loom::theme::builtin
{

// Terminal — phosphor green, monospace, zero curves, for the C++ enjoyer
inline const ThemeDef terminal = {
    .light = {{"#f0f0e6"}, {"#1b1b1b"}, {"#5c5c5c"}, {"#c0c0b0"}, {"#00875a"}},
    .dark  = {{"#080c10"}, {"#b0bec5"}, {"#546e7a"}, {"#162020"}, {"#00e676"}},
    .font  = {"ui-monospace,SFMono-Regular,Menlo,Consolas,Liberation Mono,monospace"},
    .font_size = "14px",
    .max_width = "820px",
    .extra_css = R"CSS(

/* --- zero radii everywhere --- */
.post-card,
.series-nav,
.theme-toggle,
.post-content pre, .page-content pre,
.post-content :not(pre) > code, .page-content :not(pre) > code,
.post-content img, .page-content img {
  border-radius: 0;
}

/* --- selection --- */
::selection {
  background: #00875a;
  color: #f0f0e6;
}

[data-theme="dark"] ::selection {
  background: #00e676;
  color: #080c10;
}

/* --- custom scrollbar (dark mode glow) --- */
[data-theme="dark"] ::-webkit-scrollbar {
  width: 8px;
  height: 8px;
}

[data-theme="dark"] ::-webkit-scrollbar-track {
  background: #080c10;
}

[data-theme="dark"] ::-webkit-scrollbar-thumb {
  background: #1a3a2a;
  border: 1px solid #00e67620;
}

[data-theme="dark"] ::-webkit-scrollbar-thumb:hover {
  background: #00e67640;
}

/* --- tags: <angle_bracket> syntax --- */
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
  transition: all 0.15s;
}

.tag::before { content: "<"; opacity: 0.4; }
.tag::after  { content: ">"; opacity: 0.4; }

[data-theme="dark"] .tag:hover {
  box-shadow: 0 0 8px #00e67630;
}

/* --- header: namespace :: --- */
header h1 {
  letter-spacing: 3px;
  font-weight: 700;
  text-transform: lowercase;
}

header h1::after {
  content: "::";
  opacity: 0.25;
  margin-left: 2px;
}

/* --- nav links: --flag style --- */
nav a {
  letter-spacing: 0.3px;
}

nav a::before {
  content: "--";
  opacity: 0.3;
  margin-right: 1px;
}

/* --- dark mode: phosphor glow on accent elements --- */
[data-theme="dark"] header h1 a {
  text-shadow: 0 0 20px #00e67615;
}

[data-theme="dark"] .post-content a:hover,
[data-theme="dark"] .page-content a:hover {
  text-shadow: 0 0 8px #00e67640;
}

/* --- post cards --- */
.post-card {
  border-left: 3px solid var(--accent);
  transition: border-color 0.15s, background 0.15s, box-shadow 0.15s;
}

.post-card:hover {
  border-left-color: var(--text);
  background: color-mix(in srgb, var(--accent) 4%, var(--bg));
}

[data-theme="dark"] .post-card:hover {
  box-shadow: inset 0 0 30px #00e67606;
}

/* --- post title --- */
.post-full h1 {
  font-weight: 700;
  letter-spacing: -0.3px;
  line-height: 1.3;
}

/* --- listings: prompt indicator --- */
.post-listing {
  transition: background 0.12s;
  padding: 12px 10px;
  margin: 0 -10px;
}

.post-listing:hover {
  background: color-mix(in srgb, var(--accent) 4%, var(--bg));
}

.post-listing > a::before {
  content: "> ";
  opacity: 0;
  color: var(--accent);
  transition: opacity 0.15s;
  margin-left: -1ch;
  position: absolute;
}

.post-listing:hover > a::before {
  opacity: 0.6;
}

.post-listing > a {
  position: relative;
  transition: padding-left 0.15s;
}

.post-listing:hover > a {
  padding-left: 2ch;
}

.post-listing .date {
  font-variant-numeric: tabular-nums;
}

/* --- links: dotted man-page underline --- */
.post-content a, .page-content a {
  color: var(--accent);
  text-decoration: underline;
  text-decoration-style: dotted;
  text-underline-offset: 3px;
}

.post-content a:hover, .page-content a:hover {
  text-decoration-style: solid;
}

/* --- code blocks: terminal window --- */
.post-content pre, .page-content pre {
  border: 1px solid var(--border);
  border-left: 3px solid var(--accent);
  background: color-mix(in srgb, var(--text) 4%, var(--bg));
  position: relative;
}

[data-theme="dark"] .post-content pre,
[data-theme="dark"] .page-content pre {
  background: #060a0e;
  border-color: #162020;
  border-left-color: #00e67650;
  box-shadow: inset 0 0 40px #00e67604;
}

.post-content :not(pre) > code, .page-content :not(pre) > code {
  border: 1px solid var(--border);
  background: color-mix(in srgb, var(--text) 5%, var(--bg));
  padding: 1px 5px;
  font-size: 0.9em;
}

[data-theme="dark"] .post-content :not(pre) > code,
[data-theme="dark"] .page-content :not(pre) > code {
  background: #0c1418;
  border-color: #1a2a2a;
  color: #00e676;
}

/* --- blockquotes: // comment style --- */
.post-content blockquote, .page-content blockquote {
  border-left: 2px solid var(--muted);
  font-style: italic;
  opacity: 0.85;
}

.post-content blockquote::before, .page-content blockquote::before {
  content: "// ";
  font-style: normal;
  opacity: 0.35;
  font-size: 0.85em;
}

/* --- hr: === divider --- */
.post-content hr, .page-content hr {
  border: none;
  text-align: center;
  margin: 32px 0;
}

.post-content hr::after, .page-content hr::after {
  content: "════════════════════════════════";
  color: var(--border);
  font-size: 12px;
  letter-spacing: 2px;
  overflow: hidden;
  display: block;
  max-width: 100%;
}

/* --- tables --- */
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

/* --- series nav: accent stripe --- */
.series-nav {
  border-left-color: var(--accent);
}

[data-theme="dark"] .series-nav {
  background: #0a1210;
  border-left-color: #00e67650;
}

/* --- sidebar widget headings: # prefix --- */
.widget h3::before {
  content: "# ";
  opacity: 0.3;
}

/* --- theme toggle --- */
.theme-toggle {
  border: 1px solid var(--border);
}

/* --- cursor blink in footer --- */
footer::after {
  content: "_";
  animation: blink 1s step-end infinite;
  opacity: 0.4;
  margin-left: 4px;
}

@keyframes blink {
  50% { opacity: 0; }
}

/* --- dark mode: subtle scanline texture --- */
[data-theme="dark"] body::before {
  content: "";
  position: fixed;
  inset: 0;
  background: repeating-linear-gradient(
    transparent 0px,
    transparent 2px,
    #00e67602 2px,
    #00e67602 4px
  );
  pointer-events: none;
  z-index: 9999;
}

)CSS",
};

} // namespace loom::theme::builtin
