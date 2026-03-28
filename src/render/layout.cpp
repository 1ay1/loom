#include "../../include/loom/render/layout.hpp"
#include "../../include/loom/render/themes.hpp"
#include "../../include/loom/render/dom.hpp"
#include "../../include/loom/render/base_styles.hpp"

namespace loom
{

using namespace dom;

// ═══════════════════════════════════════════════════════════════════════
//  Static assets
// ═══════════════════════════════════════════════════════════════════════

// Base CSS compiled from the typed DSL (base_styles.hpp)
static const std::string DEFAULT_CSS = compile_base_styles();

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
