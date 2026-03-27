#include "../../include/loom/render/layout.hpp"
#include "../../include/loom/render/themes.hpp"
#include "../../include/loom/render/dom.hpp"

namespace loom
{

using namespace dom;

// ═══════════════════════════════════════════════════════════════════════
//  Static assets (CSS & JS as raw strings)
// ═══════════════════════════════════════════════════════════════════════

static const char* DEFAULT_CSS = R"CSS(
:root {
  --bg: #ffffff;
  --text: #0f172a;
  --muted: #64748b;
  --border: #e5e7eb;
  --accent: #2563eb;
  --font: system-ui,-apple-system,Segoe UI,Roboto,sans-serif;
  --max-width: 700px;
  --font-size: 17px;
  --heading-font: inherit;
  --code-font: ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;
  --line-height: 1.7;
  --heading-weight: 700;
  --border-radius: 6px;
  --container-padding: 40px 20px;
  --sidebar-width: 260px;
  --sidebar-gap: 32px;
  --nav-gap: 18px;
  --header-size: 42px;
  --tag-radius: 12px;
  --tag-bg: color-mix(in srgb, var(--text) 7%, var(--bg));
  --tag-text: var(--text);
  --tag-hover-bg: color-mix(in srgb, var(--accent) 20%, var(--tag-bg));
  --tag-hover-text: var(--accent);
  --link-decoration: underline;
  --card-bg: var(--bg);
  --card-border: var(--border);
  --card-radius: 8px;
  --card-padding: 20px;
  --accent-hover: var(--accent);
  --header-border-width: 1px;
  --footer-border-width: 1px;
  --content-width: var(--max-width);
  --card-hover-shadow: 0 2px 12px color-mix(in srgb, var(--text) 6%, transparent);
  --card-hover-lift: translateY(-1px);
}

[data-theme="dark"] {
  --bg: #0b0f14;
  --text: #e5e7eb;
  --muted: #94a3b8;
  --border: #1f2937;
  --accent: #60a5fa;
}

* {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
}

body {
  background: var(--bg);
  color: var(--text);
  font-family: var(--font);
  line-height: var(--line-height);
  font-size: var(--font-size);
  overflow-x: hidden;
  -webkit-text-size-adjust: 100%;
  -webkit-font-smoothing: antialiased;
  -moz-osx-font-smoothing: grayscale;
  min-height: 100dvh;
  display: flex;
  flex-direction: column;
}

body, header, footer, .post-card, .tag, .theme-toggle, .sidebar {
  transition: background-color 0.25s, color 0.25s, border-color 0.25s;
}

::selection {
  background: var(--accent);
  color: var(--bg);
}

:focus-visible {
  outline: 2px solid var(--accent);
  outline-offset: 2px;
}

a {
  transition: color 0.15s;
}

.container {
  max-width: var(--content-width);
  margin-inline: auto;
  padding: var(--container-padding);
  width: 100%;
}

.has-sidebar .container {
  max-width: 960px;
}

header {
  border-bottom: var(--header-border-width) solid var(--border);
}

.header-bar {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.header-centered .header-bar {
  flex-direction: column;
  text-align: center;
  gap: 8px;
}

.header-minimal header {
  border-bottom: none;
}

.header-left {
  display: flex;
  flex-direction: column;
}

.header-centered .header-left {
  align-items: center;
}

header h1 {
  font-size: var(--header-size);
  font-weight: var(--heading-weight);
  font-family: var(--heading-font);
}

header h1 a {
  text-decoration: none;
  color: inherit;
}

.site-description {
  color: var(--muted);
  font-size: 15px;
  margin-top: 4px;
}

nav {
  margin-top: 10px;
}

nav ul {
  list-style: none;
  display: flex;
  gap: var(--nav-gap);
}

.header-centered nav ul {
  justify-content: center;
}

nav a {
  text-decoration: none;
  color: var(--muted);
  font-weight: 500;
}

nav a:hover {
  color: var(--accent-hover);
}

h2 {
  margin-top: 24px;
  margin-bottom: 14px;
  font-size: 20px;
  font-family: var(--heading-font);
  font-weight: var(--heading-weight);
}

html {
  scroll-behavior: smooth;
}

.post-listing {
  padding: 14px 10px;
  margin-inline: -10px;
  border-bottom: 1px solid var(--border);
  display: flex;
  flex-wrap: wrap;
  align-items: baseline;
  gap: 6px;
  border-radius: 4px;
  transition: background 0.15s;
}

.post-listing:hover {
  background: color-mix(in srgb, var(--text) 3%, var(--bg));
}

.post-listing a {
  color: var(--text);
  text-decoration: none;
  font-weight: 500;
}

.post-listing a:hover {
  color: var(--accent-hover);
}

.post-listing .post-tags {
  margin-top: 0;
}

.post-listing .tag {
  color: var(--tag-text);
  text-decoration: none;
}

.post-listing .tag:hover {
  color: var(--tag-hover-text);
}

.post-cards {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
  gap: 20px;
}

.post-card {
  background: var(--card-bg);
  border: 1px solid var(--card-border);
  border-radius: var(--card-radius);
  padding: var(--card-padding);
  transition: border-color 0.2s, box-shadow 0.2s, transform 0.2s;
}

.post-card:hover {
  border-color: var(--accent);
  box-shadow: var(--card-hover-shadow);
  transform: var(--card-hover-lift);
}

.post-card a {
  color: var(--text);
  text-decoration: none;
  font-weight: 600;
  font-size: 1.05em;
}

.post-card a:hover {
  color: var(--accent-hover);
}

.post-card .date {
  display: block;
  margin-top: 8px;
}

.post-card .post-tags {
  margin-top: 10px;
}

.post-card .tag {
  color: var(--tag-text);
  text-decoration: none;
}

.post-card .tag:hover {
  color: var(--tag-hover-text);
}

.post-full {
  padding: 0;
}

.post-full h1 {
  font-size: 1.8em;
  margin-bottom: 8px;
  font-family: var(--heading-font);
  font-weight: var(--heading-weight);
}

.post-meta {
  margin-bottom: 12px;
  font-size: 14px;
  color: var(--muted);
}

.post-full .post-tags {
  margin-bottom: 20px;
}

.post-content, .page-content {
  margin-top: 20px;
  line-height: 1.8;
}

.post-content h1, .page-content h1 {
  font-size: 1.8em;
  margin-top: 36px;
  margin-bottom: 12px;
  font-weight: var(--heading-weight);
  font-family: var(--heading-font);
}

.post-content h2, .page-content h2 {
  font-size: 1.5em;
  margin-top: 28px;
  margin-bottom: 10px;
  font-family: var(--heading-font);
}

.post-content h3, .page-content h3 {
  font-size: 1.25em;
  margin-top: 22px;
  margin-bottom: 8px;
  font-family: var(--heading-font);
}

.post-content h4, .page-content h4 {
  font-size: 1.1em;
  margin-top: 18px;
  margin-bottom: 6px;
}

.post-content h5, .page-content h5 {
  font-size: 1em;
  margin-top: 16px;
  margin-bottom: 6px;
}

.post-content h6, .page-content h6 {
  font-size: 0.9em;
  margin-top: 14px;
  margin-bottom: 4px;
  color: var(--muted);
}

.post-content h1, .post-content h2, .post-content h3,
.post-content h4, .post-content h5, .post-content h6,
.page-content h1, .page-content h2, .page-content h3,
.page-content h4, .page-content h5, .page-content h6 {
  position: relative;
  scroll-margin-top: 24px;
}

.heading-anchor {
  opacity: 0;
  text-decoration: none;
  padding-right: 0.4em;
  margin-left: -1.2em;
  font-weight: 400;
  color: var(--muted);
  transition: opacity 0.15s;
}

h1:hover > .heading-anchor,
h2:hover > .heading-anchor,
h3:hover > .heading-anchor,
h4:hover > .heading-anchor,
h5:hover > .heading-anchor,
h6:hover > .heading-anchor {
  opacity: 0.5;
}

.heading-anchor:hover {
  opacity: 1 !important;
  color: var(--accent);
}

.post-content p, .page-content p {
  margin-bottom: 14px;
}

.post-content ul, .page-content ul {
  margin-left: 24px;
  margin-bottom: 14px;
}

.post-content ul ul, .post-content ul ol,
.post-content ol ul, .post-content ol ol,
.page-content ul ul, .page-content ul ol,
.page-content ol ul, .page-content ol ol {
  margin-bottom: 0;
}

.post-content li, .page-content li {
  margin-bottom: 2px;
}

.post-content pre, .page-content pre {
  background: color-mix(in srgb, var(--text) 7%, var(--bg));
  padding: 14px;
  border-radius: var(--border-radius);
  overflow-x: auto;
  margin-bottom: 14px;
  font-size: 13px;
  line-height: 1.5;
}

.post-content code, .page-content code {
  font-family: var(--code-font);
}

.post-content :not(pre) > code, .page-content :not(pre) > code {
  background: color-mix(in srgb, var(--text) 7%, var(--bg));
  padding: 2px 6px;
  border-radius: calc(var(--border-radius) * 0.67);
  font-size: 0.9em;
}

.post-content hr, .page-content hr {
  border: none;
  border-top: 1px solid var(--border);
  margin: 28px 0;
}

.post-content dl, .page-content dl {
  margin-bottom: 14px;
}

.post-content dt, .page-content dt {
  font-weight: 600;
  margin-top: 10px;
}

.post-content dd, .page-content dd {
  margin-left: 24px;
  margin-bottom: 6px;
}

.post-content a, .page-content a {
  color: var(--accent);
  text-decoration: var(--link-decoration);
}

.post-content table, .page-content table {
  border-collapse: collapse;
  margin-bottom: 14px;
  width: 100%;
}

.post-content th, .page-content th {
  background: color-mix(in srgb, var(--text) 7%, var(--bg));
  font-weight: 600;
}

.post-content th, .post-content td,
.page-content th, .page-content td {
  border: 1px solid var(--border);
  padding: 8px 12px;
  text-align: left;
}

.post-content blockquote, .page-content blockquote {
  border-left: 4px solid var(--accent);
  margin: 0 0 14px 0;
  padding: 8px 16px;
  color: var(--muted);
}

.post-content blockquote p, .page-content blockquote p {
  margin-bottom: 4px;
}

.post-content ol, .page-content ol {
  margin-left: 24px;
  margin-bottom: 14px;
}

.post-content img, .page-content img {
  max-width: 100%;
  height: auto;
  border-radius: var(--border-radius);
  display: block;
  margin-bottom: 14px;
}

.post-content del, .page-content del {
  color: var(--muted);
}

.post-content .footnotes {
  font-size: 14px;
  color: var(--muted);
}

.post-content li:has(input[type="checkbox"]),
.page-content li:has(input[type="checkbox"]) {
  list-style: none;
  margin-left: -20px;
}

.post-content input[type="checkbox"] {
  margin-right: 6px;
}

time {
  color: var(--muted);
  font-size: 14px;
}

.date {
  color: var(--muted);
  font-size: 14px;
  font-family: var(--code-font);
  font-variant-numeric: tabular-nums;
}

footer {
  margin-top: auto;
  padding-top: 40px;
  padding-bottom: 24px;
  border-top: var(--footer-border-width) solid var(--border);
  font-size: 14px;
  color: var(--muted);
}

.footer-links {
  margin-top: 8px;
  display: flex;
  gap: 16px;
}

.footer-links a {
  color: var(--muted);
  text-decoration: none;
}

.footer-links a:hover {
  color: var(--accent-hover);
}

.post-tags {
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
  margin-top: 8px;
}

.tag {
  display: inline-block;
  background: var(--tag-bg);
  color: var(--tag-text);
  padding: 2px 10px;
  border-radius: var(--tag-radius);
  font-size: 13px;
  text-decoration: none;
  font-weight: 500;
}

.tag:hover {
  background: var(--tag-hover-bg);
  color: var(--tag-hover-text);
}

.with-sidebar {
  display: grid;
  grid-template-columns: 1fr var(--sidebar-width);
  gap: var(--sidebar-gap);
}

.sidebar-left .with-sidebar {
  grid-template-columns: var(--sidebar-width) 1fr;
}

.sidebar-left .with-sidebar > aside {
  order: -1;
}

.with-sidebar > main {
  min-width: 0;
  overflow-wrap: break-word;
  word-break: break-word;
}

.sidebar {
  font-size: 14px;
  border-left: 1px solid var(--border);
  padding-left: 32px;
  padding-top: 8px;
}

.sidebar-left .sidebar {
  border-left: none;
  border-right: 1px solid var(--border);
  padding-left: 0;
  padding-right: 32px;
}

.widget {
  margin-bottom: 24px;
}

.widget h3 {
  font-size: 13px;
  font-weight: 600;
  margin-bottom: 10px;
  text-transform: uppercase;
  letter-spacing: 0.8px;
  color: var(--muted);
  border-bottom: 1px solid var(--border);
  padding-bottom: 6px;
}

.widget ul {
  list-style: none;
}

.widget li {
  margin-bottom: 8px;
  line-height: 1.4;
}

.sidebar .post-tags {
  gap: 5px;
}

.sidebar .tag {
  font-size: 12px;
  padding: 1px 7px;
}

.widget li a {
  color: var(--text);
  text-decoration: none;
  font-size: 14px;
}

.widget li a:hover {
  color: var(--accent-hover);
}

.widget .date {
  display: block;
  font-size: 12px;
}

.widget p {
  color: var(--muted);
  line-height: 1.6;
  font-size: 14px;
}

.widget p a {
  color: var(--accent);
  text-decoration: none;
}

.widget p a:hover {
  color: var(--accent-hover);
  text-decoration: underline;
}

.widget .post-tags {
  margin-top: 0;
}

@media (max-width: 768px) {
  :root {
    --container-padding: 24px 16px;
    --font-size: 15px;
    --header-size: 36px;
    --line-height: 1.65;
  }

  nav ul {
    flex-wrap: wrap;
    gap: 10px 14px;
  }

  .with-sidebar {
    grid-template-columns: 1fr;
  }
  .sidebar-left .with-sidebar {
    grid-template-columns: 1fr;
  }
  .sidebar-left .with-sidebar > aside {
    order: initial;
  }
  .sidebar {
    border-left: none;
    border-top: 1px solid var(--border);
    padding-left: 0;
    padding-top: 24px;
  }
  .sidebar-left .sidebar {
    border-right: none;
    border-top: 1px solid var(--border);
    padding-right: 0;
    padding-top: 24px;
  }
  .has-sidebar .container {
    max-width: var(--content-width);
  }

  .post-full h1 {
    font-size: 1.5em;
  }

  .post-content h1, .page-content h1 {
    font-size: 1.5em;
    margin-top: 28px;
  }

  .post-content h2, .page-content h2 {
    font-size: 1.3em;
    margin-top: 22px;
  }

  .post-content h3, .page-content h3 {
    font-size: 1.15em;
    margin-top: 18px;
  }

  .post-content pre, .page-content pre {
    padding: 12px;
    font-size: 13px;
    border-radius: 4px;
    margin-left: -16px;
    margin-right: -16px;
    border-radius: 0;
  }

  .post-content table, .page-content table {
    display: block;
    overflow-x: auto;
    -webkit-overflow-scrolling: touch;
  }

  .post-content, .page-content {
    font-size: 14.5px;
    line-height: 1.7;
  }

  .post-content blockquote, .page-content blockquote {
    margin-left: 0;
    margin-right: 0;
    padding: 6px 12px;
  }

  .post-cards {
    grid-template-columns: 1fr;
    gap: 14px;
  }

  .footer-links {
    flex-wrap: wrap;
    gap: 10px 16px;
  }
}

@media (max-width: 480px) {
  :root {
    --container-padding: 20px 12px;
    --font-size: 14.5px;
    --header-size: 30px;
  }

  .post-content, .page-content {
    font-size: 13.5px;
  }

  .post-full h1 {
    font-size: 1.35em;
  }

  .post-content pre, .page-content pre {
    margin-left: -12px;
    margin-right: -12px;
    font-size: 12px;
    padding: 10px;
  }

  .post-content th, .post-content td,
  .page-content th, .page-content td {
    padding: 6px 8px;
    font-size: 14px;
  }

  .tag {
    font-size: 12px;
    padding: 2px 8px;
  }
}

.theme-toggle {
  cursor: pointer;
  border: 1px solid var(--border);
  background: none;
  color: var(--text);
  padding: 6px 12px;
  border-radius: var(--border-radius);
  font-size: 14px;
  transition: background 0.2s ease;
}

.theme-toggle:hover {
  background: var(--border);
}

.reading-time {
  color: var(--muted);
  font-size: 13px;
}

.excerpt {
  color: var(--muted);
  font-size: 14px;
  line-height: 1.5;
  margin-top: 6px;
  margin-bottom: 0;
}

.post-card .excerpt {
  display: -webkit-box;
  -webkit-line-clamp: 3;
  -webkit-box-orient: vertical;
  overflow: hidden;
}

.post-nav {
  display: flex;
  justify-content: space-between;
  gap: 20px;
  margin-top: 48px;
  padding-top: 20px;
  border-top: 1px solid var(--border);
}

.post-nav a {
  color: var(--muted);
  text-decoration: none;
  font-size: 14px;
  max-width: 45%;
  line-height: 1.4;
  transition: color 0.15s;
}

.post-nav a:hover {
  color: var(--accent);
}

.post-nav-next {
  text-align: right;
  margin-left: auto;
}

.related-posts {
  margin-top: 32px;
  padding-top: 20px;
  border-top: 1px solid var(--border);
}

.related-posts h2 {
  font-size: 14px;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.5px;
  color: var(--muted);
  margin-top: 0;
  margin-bottom: 12px;
}

.related-posts ul {
  list-style: none;
}

.related-posts li {
  margin-bottom: 8px;
  line-height: 1.4;
}

.related-posts li a {
  color: var(--text);
  text-decoration: none;
  font-weight: 500;
}

.related-posts li a:hover {
  color: var(--accent);
}

.related-posts .date {
  font-size: 12px;
  margin-left: 8px;
}

.series-nav {
  margin: 24px 0;
  padding: 16px 20px;
  border: 1px solid var(--border);
  border-left: 3px solid var(--accent);
  border-radius: var(--border-radius);
  font-size: 14px;
  background: color-mix(in srgb, var(--accent) 3%, var(--bg));
}

.series-label {
  margin-bottom: 10px;
  font-size: 13px;
  color: var(--muted);
}

.series-list {
  margin-left: 20px;
  margin-bottom: 0;
}

.series-list li {
  margin-bottom: 4px;
}

.series-list a {
  color: var(--accent);
  text-decoration: none;
}

.series-list a:hover {
  text-decoration: underline;
}

.series-current {
  font-weight: 600;
  color: var(--text);
}

.archive-year {
  margin-top: 24px;
}

nav.breadcrumb {
  font-size: 0.85rem;
  color: var(--muted);
  margin-bottom: 20px;
}

nav.breadcrumb ol {
  list-style: none;
  padding: 0;
  margin: 0;
  display: flex;
  flex-wrap: wrap;
  gap: 4px;
  align-items: center;
}

nav.breadcrumb li + li::before {
  content: "\203A";
  margin-right: 4px;
  color: var(--muted);
}

nav.breadcrumb a {
  color: var(--muted);
  text-decoration: none;
}

nav.breadcrumb a:hover {
  color: var(--accent);
  text-decoration: underline;
}

nav.breadcrumb li:last-child {
  color: var(--text);
}

@media print {
  body {
    font-size: 12pt;
    color: #000;
    background: #fff;
    display: block;
  }

  header, footer, .theme-toggle, .sidebar, .post-nav,
  .breadcrumb, .related-posts, .post-tags {
    display: none;
  }

  .container {
    max-width: 100%;
    padding: 0;
  }

  a {
    color: #000;
    text-decoration: underline;
  }

  .post-content pre, .page-content pre {
    border: 1px solid #ccc;
    page-break-inside: avoid;
  }

  .post-content img, .page-content img {
    max-width: 100%;
    page-break-inside: avoid;
  }

  .post-full h1 {
    font-size: 24pt;
    margin-bottom: 8pt;
  }

  .post-meta {
    color: #666;
    margin-bottom: 16pt;
  }
}
)CSS";

static const char* THEME_JS = R"JS(
    function applyTheme(theme) {
      document.documentElement.setAttribute("data-theme", theme);
      var btn = document.getElementById("themeToggle");
      if (btn) btn.textContent = theme === "dark" ? "\u2600\uFE0F" : "\uD83C\uDF19";
    }

    function toggleTheme() {
      var current = document.documentElement.getAttribute("data-theme");
      var next = current === "dark" ? "light" : "dark";
      applyTheme(next);
      localStorage.setItem("theme", next);
    }

    var saved = localStorage.getItem("theme");
    if (saved) {
      applyTheme(saved);
    } else {
      var systemDark = window.matchMedia("(prefers-color-scheme: dark)").matches;
      applyTheme(systemDark ? "dark" : "light");
    }

    window.matchMedia("(prefers-color-scheme: dark)")
      .addEventListener("change", function(e) {
        if (!localStorage.getItem("theme")) {
          applyTheme(e.matches ? "dark" : "light");
        }
      });
)JS";

// ═══════════════════════════════════════════════════════════════════════
//  Utilities
// ═══════════════════════════════════════════════════════════════════════

static std::string escape_json(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s)
    {
        switch (c)
        {
            case '"': out += "&quot;"; break;
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            default: out += c;
        }
    }
    return out;
}

static std::string apply_external_new_tab(const std::string& html)
{
    static const char ATTRS[] = " target=\"_blank\" rel=\"noopener noreferrer\"";
    std::string result;
    result.reserve(html.size() + html.size() / 16);

    size_t pos = 0;
    while (true)
    {
        size_t a_start = html.find("<a ", pos);
        if (a_start == std::string::npos) { result.append(html, pos, std::string::npos); break; }
        size_t tag_end = html.find('>', a_start);
        if (tag_end == std::string::npos) { result.append(html, pos, std::string::npos); break; }

        bool is_external = false, has_target = false;
        for (size_t k = a_start + 3; k < tag_end; ++k)
        {
            if (!is_external && html[k] == 'h' && html.compare(k, 10, "href=\"http") == 0) is_external = true;
            if (html[k] == 't' && html.compare(k, 7, "target=") == 0) { has_target = true; break; }
        }

        if (is_external && !has_target)
        { result.append(html, pos, tag_end - pos); result += ATTRS; result += '>'; pos = tag_end + 1; }
        else
        { result.append(html, pos, tag_end + 1 - pos); pos = tag_end + 1; }
    }
    return result;
}

// ═══════════════════════════════════════════════════════════════════════
//  Components — each returns a dom::Node
// ═══════════════════════════════════════════════════════════════════════

// ── JSON-LD structured data ──

static Node json_ld(const std::string& json)
{
    return script(attr("type", "application/ld+json"), raw(json));
}

static Node build_structured_data(const Site& site, const PageMeta& page_meta,
                                   const std::string& canonical_url, const std::string& og_image_url)
{
    auto esc = escape_json;
    std::vector<Node> nodes;

    if (page_meta.og_type == "article")
    {
        std::string j = "{\"@context\":\"https://schema.org\",\"@type\":\"BlogPosting\",";
        j += "\"headline\":\"" + esc(page_meta.title) + "\",";
        if (!page_meta.published_date.empty()) j += "\"datePublished\":\"" + page_meta.published_date + "\",";
        if (!site.author.empty()) j += "\"author\":{\"@type\":\"Person\",\"name\":\"" + esc(site.author) + "\"},";
        if (!site.base_url.empty()) j += "\"url\":\"" + esc(canonical_url) + "\",";
        if (!og_image_url.empty()) j += "\"image\":\"" + esc(og_image_url) + "\",";
        j += "\"publisher\":{\"@type\":\"Organization\",\"name\":\"" + esc(site.title) + "\"}}";
        nodes.push_back(json_ld(j));
    }
    else if (page_meta.canonical_path.empty() || page_meta.canonical_path == "/")
    {
        std::string j = "{\"@context\":\"https://schema.org\",\"@type\":\"WebSite\",";
        j += "\"name\":\"" + esc(site.title) + "\",";
        if (!site.base_url.empty()) j += "\"url\":\"" + site.base_url + "\",";
        j += "\"description\":\"" + esc(site.description) + "\"}";
        nodes.push_back(json_ld(j));
    }

    // BreadcrumbList
    struct Crumb { std::string name, path; };
    std::vector<Crumb> crumbs;
    const auto& cp = page_meta.canonical_path;
    if (page_meta.og_type == "article") crumbs = {{"Home", "/"}, {page_meta.title, cp}};
    else if (cp == "/tags") crumbs = {{"Home", "/"}, {"Tags", "/tags"}};
    else if (cp.size() > 5 && cp.substr(0, 5) == "/tag/") crumbs = {{"Home", "/"}, {"Tags", "/tags"}, {cp.substr(5), cp}};
    else if (cp == "/series") crumbs = {{"Home", "/"}, {"Series", "/series"}};
    else if (cp.size() > 8 && cp.substr(0, 8) == "/series/") crumbs = {{"Home", "/"}, {"Series", "/series"}, {cp.substr(8), cp}};

    if (!crumbs.empty() && !site.base_url.empty())
    {
        std::string j = "{\"@context\":\"https://schema.org\",\"@type\":\"BreadcrumbList\",\"itemListElement\":[";
        for (size_t i = 0; i < crumbs.size(); ++i)
        {
            if (i > 0) j += ',';
            j += "{\"@type\":\"ListItem\",\"position\":" + std::to_string(i + 1);
            j += ",\"name\":\"" + esc(crumbs[i].name) + "\"";
            j += ",\"item\":\"" + esc(site.base_url + crumbs[i].path) + "\"}";
        }
        j += "]}";
        nodes.push_back(json_ld(j));
    }

    Node f{Node::Fragment};
    f.children = std::move(nodes);
    return f;
}

// ── <head> ──

static Node build_head(const Site& site, const PageMeta& page_meta)
{
    const auto& layout = site.layout;

    std::string og_image_url;
    if (!page_meta.og_image.empty())
    {
        if (page_meta.og_image.find("://") != std::string::npos) og_image_url = page_meta.og_image;
        else if (!site.base_url.empty()) og_image_url = site.base_url + (page_meta.og_image.front() == '/' ? page_meta.og_image : "/" + page_meta.og_image);
        else og_image_url = page_meta.og_image;
    }

    std::string page_title = page_meta.title.empty() ? site.title : page_meta.title + " \xe2\x80\x94 " + site.title;
    std::string page_desc  = page_meta.description.empty() ? site.description : page_meta.description;
    std::string canonical   = site.base_url + page_meta.canonical_path;
    std::string og_title    = page_meta.title.empty() ? site.title : page_meta.title;

    // Build theme CSS
    std::string theme_css;
    if (site.theme.name != "default" && site.theme.name != "custom")
    {
        auto& themes = builtin_themes();
        auto it = themes.find(site.theme.name);
        if (it != themes.end()) theme_css = theme::compile(it->second);
    }
    std::string var_css;
    if (!site.theme.variables.empty())
    {
        std::string lv, dv;
        for (const auto& [key, value] : site.theme.variables)
        {
            if (key.substr(0, 5) == "dark-") dv += "--" + key.substr(5) + ":" + value + ";";
            else lv += "--" + key + ":" + value + ";";
        }
        if (!lv.empty()) var_css += ":root{" + lv + "}";
        if (!dv.empty()) var_css += "[data-theme=\"dark\"]{" + dv + "}";
    }

    return head(
        meta(attr("charset", "utf-8")),
        meta(name("viewport"), attr("content", "width=device-width, initial-scale=1")),
        title(page_title),
        meta(name("description"), attr("content", page_desc)),

        // Robots
        when(page_meta.noindex, meta(name("robots"), attr("content", "noindex, nofollow"))),

        // Author & canonical
        when(!site.author.empty(), meta(name("author"), attr("content", site.author))),
        when(!site.base_url.empty(), link(rel("canonical"), href(canonical))),

        // Preload hero image
        when(page_meta.og_type == "article" && !og_image_url.empty(),
            link(rel("preload"), attr("as", "image"), href(og_image_url))),

        // Open Graph
        meta(attr("property", "og:type"), attr("content", page_meta.og_type.empty() ? "website" : page_meta.og_type)),
        meta(attr("property", "og:title"), attr("content", og_title)),
        meta(attr("property", "og:description"), attr("content", page_desc)),
        when(!site.base_url.empty(), fragment(
            meta(attr("property", "og:url"), attr("content", canonical)),
            meta(attr("property", "og:site_name"), attr("content", site.title)))),
        when(!site.author.empty() && page_meta.og_type == "article",
            meta(attr("property", "article:author"), attr("content", site.author))),
        when(!page_meta.published_date.empty(),
            meta(attr("property", "article:published_time"), attr("content", page_meta.published_date))),
        each(page_meta.tags, [](const std::string& t) {
            return meta(attr("property", "article:tag"), attr("content", t)); }),
        when(!og_image_url.empty(),
            meta(attr("property", "og:image"), attr("content", og_image_url))),

        // Twitter Card
        meta(name("twitter:card"), attr("content", og_image_url.empty() ? "summary" : "summary_large_image")),
        meta(name("twitter:title"), attr("content", og_title)),
        meta(name("twitter:description"), attr("content", page_desc)),
        when(!og_image_url.empty(),
            meta(name("twitter:image"), attr("content", og_image_url))),

        // RSS
        when(!site.base_url.empty(),
            link(rel("alternate"), type("application/rss+xml"),
                 attr("title", site.title + " RSS"), href(site.base_url + "/feed.xml"))),

        // Structured data
        build_structured_data(site, page_meta, canonical, og_image_url),

        // Theme JS
        when(layout.show_theme_toggle, script(raw(THEME_JS))),

        // Custom head HTML
        when(!layout.custom_head_html.empty(), raw(layout.custom_head_html)),

        // Styles
        style(raw(site.theme.css.empty() ? DEFAULT_CSS : site.theme.css)),
        when(!theme_css.empty(), style(raw(theme_css))),
        when(!var_css.empty(), style(raw(var_css))),
        when(!layout.custom_css.empty(), style(raw(layout.custom_css)))
    );
}

// ── Header ──

static Node build_header(const Site& site, const std::string& nav_html)
{
    const auto& layout = site.layout;
    return header(
        div(class_("container header-bar"),
            div(class_("header-left"),
                h1(a(href("/"), site.title)),
                when(layout.show_description && !site.description.empty(),
                    p_(class_("site-description"), site.description)),
                raw(nav_html)),
            when(layout.show_theme_toggle,
                button(class_("theme-toggle"), id("themeToggle"),
                       onclick("toggleTheme()"), raw("\xF0\x9F\x8C\x99"))))
    );
}

// ── Breadcrumbs ──

static Node build_breadcrumbs(const LayoutConfig& layout, const PageMeta& page_meta)
{
    if (!layout.show_breadcrumbs) return Node{Node::Fragment, {}, {}, {}, {}};

    struct Crumb { std::string label, url; };
    std::vector<Crumb> crumbs;
    const auto& cp = page_meta.canonical_path;

    if (page_meta.og_type == "article")         crumbs = {{"Home", "/"}, {page_meta.title, ""}};
    else if (cp == "/tags")                crumbs = {{"Home", "/"}, {"Tags", ""}};
    else if (cp.size() > 5 && cp.substr(0, 5) == "/tag/")
        crumbs = {{"Home", "/"}, {"Tags", "/tags"}, {cp.substr(5), ""}};
    else if (cp == "/series")              crumbs = {{"Home", "/"}, {"Series", ""}};
    else if (cp.size() > 8 && cp.substr(0, 8) == "/series/")
        crumbs = {{"Home", "/"}, {"Series", "/series"}, {cp.substr(8), ""}};

    if (crumbs.empty()) return Node{Node::Fragment, {}, {}, {}, {}};

    return nav(class_("breadcrumb"), aria_label("Breadcrumb"),
        ol(each(crumbs, [](const Crumb& c) -> Node {
            if (!c.url.empty()) return li(a(href(c.url), c.label));
            return li(c.label);
        }))
    );
}

// ── Content area ──

static Node build_content(const std::string& content, const std::string& sidebar,
                           Node breadcrumbs)
{
    if (!sidebar.empty())
        return div(class_("container with-sidebar"),
            main_(std::move(breadcrumbs), raw(content)),
            raw(sidebar));

    return div(class_("container"),
        std::move(breadcrumbs), raw(content));
}

// ── Footer ──

static Node build_footer(const Site& site)
{
    return footer(
        div(class_("container"),
            p_(raw(!site.footer.copyright.empty() ? site.footer.copyright : "Powered by Loom")),
            when(!site.footer.links.empty(),
                div(class_("footer-links"),
                    each(site.footer.links, [](const auto& lnk) {
                        return a(href(lnk.url), lnk.title);
                    }))))
    );
}

// ═══════════════════════════════════════════════════════════════════════
//  Page layout — composes all components
// ═══════════════════════════════════════════════════════════════════════

std::string render_layout(
    const Site& site,
    const std::string& navigation,
    const std::string& content,
    const std::string& sidebar,
    const PageMeta& page_meta)
{
    const auto& layout = site.layout;

    auto page = document(
        build_head(site, page_meta),
        body(
            classes({
                {"has-sidebar",     !sidebar.empty()},
                {"sidebar-left",    layout.sidebar_position == "left"},
                {"header-centered", layout.header_style == "centered"},
                {"header-minimal",  layout.header_style == "minimal"}
            }),
            build_header(site, navigation),
            build_content(content, sidebar, build_breadcrumbs(layout, page_meta)),
            build_footer(site)
        )
    );

    auto result = page.render();

    if (layout.external_links_new_tab)
        result = apply_external_new_tab(result);

    return result;
}

}
