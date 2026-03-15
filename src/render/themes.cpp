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
        "#0a0a0a", "#d4d4d4", "#737373", "#262626", "#4ade80", {}
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
        "#0c0c0c", "#b5b5b5", "#606060", "#1a1a1a", "#88c070",

        R"CSS(
.tag {
  border-radius: 0;
  background: transparent;
  border: 1px solid var(--border);
}

.post-card {
  border-radius: 0;
}

.theme-toggle {
  border-radius: 0;
}

.post-content pre, .page-content pre {
  border: 1px solid var(--border);
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
