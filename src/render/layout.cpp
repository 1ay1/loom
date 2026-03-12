#include "../../include/loom/render/render.hpp"

namespace loom
{

std::string render_layout(
    const std::string& title,
    const std::string& navigation,
    const std::string& content)
{
    std::string html;

    html += "<!DOCTYPE html>";
    html += "<html>";
    html += "<head>";
    html += "<meta charset=\"utf-8\">";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    html += "<title>" + title + "</title>";

    html += "<style>";
    html += R"CSS(

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

/* layout */

.container {
  max-width: 720px;
  margin: auto;
  padding: 40px 20px;
}

/* header */

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

/* nav */

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

/* posts */

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

/* footer */

footer {
  margin-top: 70px;
  padding-top: 20px;
  border-top: 1px solid var(--border);
  font-size: 14px;
  color: var(--muted);
}

/* theme toggle */

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

    html += "</style>";

    html += "</head>";
    html += "<body>";

    html += "<header>";
    html += "<div class='container header-bar'>";
    html += "<div class='header-left'>";
    html += "<h1>" + title + "</h1>";
    html += navigation;
    html += "</div>";
    html += "<button class='theme-toggle' id='themeToggle' onclick='toggleTheme()'>🌙</button>";
    html += "</div>";
    html += "</header>";

    html += "<div class='container'>";
    html += content;
    html += "</div>";

    html += "<footer>";
    html += "<div class='container'>";
    html += "<p>Powered by Loom</p>";
    html += "</div>";
    html += "</footer>";

    html += "<script>";
    html += R"JS(

    function applyTheme(theme) {
      document.documentElement.setAttribute("data-theme", theme);
      const btn = document.getElementById("themeToggle");
      if (btn) btn.textContent = theme === "dark" ? "☀" : "🌙";
    }

    function toggleTheme() {
      const current = document.documentElement.getAttribute("data-theme");
      const next = current === "dark" ? "light" : "dark";

      applyTheme(next);
      localStorage.setItem("theme", next);
    }

    const saved = localStorage.getItem("theme");

    if (saved) {
      applyTheme(saved);
    } else {
      const systemDark = window.matchMedia("(prefers-color-scheme: dark)").matches;
      applyTheme(systemDark ? "dark" : "light");
    }

    window.matchMedia("(prefers-color-scheme: dark)")
      .addEventListener("change", function(e) {
        if (!localStorage.getItem("theme")) {
          applyTheme(e.matches ? "dark" : "light");
        }
      });

    )JS";
    html += "</script>";

    html += "</body>";
    html += "</html>";

    return html;
}

}
