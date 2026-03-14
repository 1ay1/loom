#include "../../include/loom/render/layout.hpp"
#include "../../include/loom/render/themes.hpp"

namespace loom
{

static const char* DEFAULT_CSS = R"CSS(
:root {
  --bg: #ffffff;
  --text: #0f172a;
  --muted: #64748b;
  --border: #e5e7eb;
  --accent: #2563eb;
  --font: system-ui,-apple-system,Segoe UI,Roboto,sans-serif;
  --max-width: 720px;
  --font-size: 17px;
  --heading-font: inherit;
  --code-font: ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;
  --line-height: 1.7;
  --heading-weight: 700;
  --border-radius: 6px;
  --container-padding: 40px 20px;
  --sidebar-width: 240px;
  --sidebar-gap: 48px;
  --nav-gap: 18px;
  --header-size: 26px;
  --tag-radius: 12px;
  --tag-bg: var(--border);
  --tag-text: var(--muted);
  --link-decoration: underline;
  --card-bg: var(--bg);
  --card-border: var(--border);
  --card-radius: 8px;
  --card-padding: 20px;
  --accent-hover: var(--accent);
  --header-border-width: 1px;
  --footer-border-width: 1px;
  --content-width: var(--max-width);
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
}

.container {
  max-width: var(--content-width);
  margin: auto;
  padding: var(--container-padding);
}

.has-sidebar .container {
  max-width: calc(var(--content-width) + var(--sidebar-width) + var(--sidebar-gap));
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

.post-listing {
  padding: 12px 0;
  border-bottom: 1px solid var(--border);
  display: flex;
  flex-wrap: wrap;
  align-items: baseline;
  gap: 6px;
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
  transition: border-color 0.2s ease;
}

.post-card:hover {
  border-color: var(--accent);
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
  border-radius: var(--border-radius);
  overflow-x: auto;
  margin-bottom: 14px;
  font-size: 15px;
}

.post-content code, .page-content code {
  font-family: var(--code-font);
}

.post-content :not(pre) > code, .page-content :not(pre) > code {
  background: var(--border);
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
  border-radius: var(--border-radius);
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
}

footer {
  margin-top: 70px;
  padding-top: 20px;
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
  background: var(--accent);
  color: var(--bg);
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

.widget .post-tags {
  margin-top: 0;
}

@media (max-width: 860px) {
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
    const std::string& content,
    const std::string& sidebar)
{
    const auto& layout = site.layout;
    std::string html;
    html.reserve(8192 + content.size());

    html += "<!DOCTYPE html>";
    html += "<html>";
    html += "<head>";
    html += "<meta charset=\"utf-8\">";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    html += "<title>" + site.title + "</title>";
    html += "<meta name=\"description\" content=\"" + site.description + "\">";

    // Custom head HTML (e.g. Google Fonts, analytics)
    if (!layout.custom_head_html.empty())
        html += layout.custom_head_html;

    html += "<style>";
    if (site.theme.css.empty())
        html += DEFAULT_CSS;
    else
        html += site.theme.css;
    html += "</style>";

    // Apply built-in theme if selected
    if (site.theme.name != "default" && site.theme.name != "custom")
    {
        auto& themes = builtin_themes();
        auto it = themes.find(site.theme.name);
        if (it != themes.end())
            html += "<style>" + theme_to_css(it->second) + "</style>";
    }

    // Emit theme variable overrides from site.conf theme_* keys
    if (!site.theme.variables.empty())
    {
        std::string light_vars, dark_vars;
        for (const auto& [key, value] : site.theme.variables)
        {
            if (key.substr(0, 5) == "dark-")
                dark_vars += "--" + key.substr(5) + ":" + value + ";";
            else
                light_vars += "--" + key + ":" + value + ";";
        }
        if (!light_vars.empty())
            html += "<style>:root{" + light_vars + "}</style>";
        if (!dark_vars.empty())
            html += "<style>[data-theme=\"dark\"]{" + dark_vars + "}</style>";
    }

    // Custom CSS from config
    if (!layout.custom_css.empty())
        html += "<style>" + layout.custom_css + "</style>";

    html += "</head>";

    // Body classes for structural variants
    std::string body_classes;
    if (!sidebar.empty())
        body_classes += " has-sidebar";
    if (layout.sidebar_position == "left")
        body_classes += " sidebar-left";
    if (layout.header_style == "centered")
        body_classes += " header-centered";
    if (layout.header_style == "minimal")
        body_classes += " header-minimal";

    if (!body_classes.empty())
        html += "<body class='" + body_classes.substr(1) + "'>";
    else
        html += "<body>";

    // Header
    html += "<header>";
    html += "<div class='container header-bar'>";
    html += "<div class='header-left'>";
    html += "<h1><a href='/'>" + site.title + "</a></h1>";
    if (layout.show_description && !site.description.empty())
        html += "<p class='site-description'>" + site.description + "</p>";
    html += navigation;
    html += "</div>";
    if (layout.show_theme_toggle)
        html += "<button class='theme-toggle' id='themeToggle' onclick='toggleTheme()'>\xF0\x9F\x8C\x99</button>";
    html += "</div>";
    html += "</header>";

    // Main content
    if (!sidebar.empty())
    {
        html += "<div class='container with-sidebar'>";
        html += "<main>";
        html += content;
        html += "</main>";
        html += sidebar;
        html += "</div>";
    }
    else
    {
        html += "<div class='container'>";
        html += content;
        html += "</div>";
    }

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
    if (layout.show_theme_toggle)
    {
        html += "<script>";
        html += THEME_JS;
        html += "</script>";
    }

    html += "</body>";
    html += "</html>";

    return html;
}

}
