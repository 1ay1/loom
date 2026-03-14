#include "../../include/loom/render/layout.hpp"

namespace loom
{

static const char* DEFAULT_CSS = R"CSS(
:root {
  --bg: #ffffff;
  --text: #0f172a;
  --muted: #64748b;
  --border: #e5e7eb;
  --accent: #2563eb;
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
  font-family: system-ui,-apple-system,Segoe UI,Roboto,sans-serif;
  line-height: 1.7;
  font-size: 17px;
}

.container {
  max-width: 720px;
  margin: auto;
  padding: 40px 20px;
}

header {
  border-bottom: 1px solid var(--border);
}

.header-bar {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.header-left {
  display: flex;
  flex-direction: column;
}

header h1 {
  font-size: 26px;
  font-weight: 700;
}

header h1 a {
  text-decoration: none;
  color: inherit;
}

nav {
  margin-top: 10px;
}

nav ul {
  list-style: none;
  display: flex;
  gap: 18px;
}

nav a {
  text-decoration: none;
  color: var(--muted);
  font-weight: 500;
}

nav a:hover {
  color: var(--accent);
}

h2 {
  margin-top: 35px;
  margin-bottom: 14px;
  font-size: 20px;
}

article {
  padding: 12px 0;
  border-bottom: 1px solid var(--border);
}

article a {
  color: var(--text);
  text-decoration: none;
  font-weight: 500;
}

article a:hover {
  color: var(--accent);
}

.post-content, .page-content {
  margin-top: 20px;
  line-height: 1.8;
}

.post-content h1, .page-content h1 {
  font-size: 1.8em;
  margin-top: 36px;
  margin-bottom: 12px;
  font-weight: 700;
}

.post-content h2, .page-content h2 {
  font-size: 1.5em;
  margin-top: 28px;
  margin-bottom: 10px;
}

.post-content h3, .page-content h3 {
  font-size: 1.25em;
  margin-top: 22px;
  margin-bottom: 8px;
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
  background: var(--border);
  padding: 16px;
  border-radius: 6px;
  overflow-x: auto;
  margin-bottom: 14px;
  font-size: 15px;
}

.post-content code, .page-content code {
  font-family: ui-monospace, monospace;
}

.post-content :not(pre) > code, .page-content :not(pre) > code {
  background: var(--border);
  padding: 2px 6px;
  border-radius: 4px;
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
  text-decoration: underline;
}

.post-content table, .page-content table {
  border-collapse: collapse;
  margin-bottom: 14px;
  width: 100%;
}

.post-content th, .page-content th {
  background: var(--border);
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

.post-content blockquote p {
  margin-bottom: 4px;
}

.post-content ol, .page-content ol {
  margin-left: 24px;
  margin-bottom: 14px;
}

.post-content img, .page-content img {
  max-width: 100%;
  height: auto;
  border-radius: 6px;
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
  font-family: ui-monospace, monospace;
}

footer {
  margin-top: 70px;
  padding-top: 20px;
  border-top: 1px solid var(--border);
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
  color: var(--accent);
}

.theme-toggle {
  cursor: pointer;
  border: 1px solid var(--border);
  background: none;
  color: var(--text);
  padding: 6px 12px;
  border-radius: 6px;
  font-size: 14px;
  transition: background 0.2s ease;
}

.theme-toggle:hover {
  background: var(--border);
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

std::string render_layout(
    const Site& site,
    const std::string& navigation,
    const std::string& content)
{
    std::string html;

    html += "<!DOCTYPE html>";
    html += "<html>";
    html += "<head>";
    html += "<meta charset=\"utf-8\">";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    html += "<title>" + site.title + "</title>";
    html += "<meta name=\"description\" content=\"" + site.description + "\">";

    html += "<style>";
    if (site.theme.css.empty())
        html += DEFAULT_CSS;
    else
        html += site.theme.css;
    html += "</style>";

    html += "</head>";
    html += "<body>";

    // Header
    html += "<header>";
    html += "<div class='container header-bar'>";
    html += "<div class='header-left'>";
    html += "<h1><a href='/'>" + site.title + "</a></h1>";
    html += navigation;
    html += "</div>";
    html += "<button class='theme-toggle' id='themeToggle' onclick='toggleTheme()'>\xF0\x9F\x8C\x99</button>";
    html += "</div>";
    html += "</header>";

    // Main content
    html += "<div class='container'>";
    html += content;
    html += "</div>";

    // Footer
    html += "<footer>";
    html += "<div class='container'>";
    if (!site.footer.copyright.empty())
        html += "<p>" + site.footer.copyright + "</p>";
    else
        html += "<p>Powered by Loom</p>";

    if (!site.footer.links.empty())
    {
        html += "<div class='footer-links'>";
        for (const auto& link : site.footer.links)
            html += "<a href=\"" + link.url + "\">" + link.title + "</a>";
        html += "</div>";
    }
    html += "</div>";
    html += "</footer>";

    // Theme toggle JS
    html += "<script>";
    html += THEME_JS;
    html += "</script>";

    html += "</body>";
    html += "</html>";

    return html;
}

}
