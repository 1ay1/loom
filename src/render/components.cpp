#include "../../include/loom/render/component.hpp"
#include "../../include/loom/render/themes.hpp"
#include "../../include/loom/render/theme/theme_def.hpp"
#include "../../include/loom/render/base_styles.hpp"

#include <ctime>
#include <cmath>

namespace loom::component
{

using namespace dom;

// ═══════════════════════════════════════════════════════════════════════
//  Utilities
// ═══════════════════════════════════════════════════════════════════════

static std::string format_date(std::chrono::system_clock::time_point tp, const std::string& fmt)
{
    auto time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
    gmtime_r(&time, &tm);
    char buf[64];
    std::strftime(buf, sizeof(buf), fmt.c_str(), &tm);
    return buf;
}

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

// Resolve date format: ThemeDef override > layout config
static const std::string& resolve_date_format(const Ctx& ctx)
{
    if (ctx.theme_def && !ctx.theme_def->date_format.empty())
        return ctx.theme_def->date_format;
    return ctx.site.layout.date_format;
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

std::vector<TocEntry> extract_toc(const std::string& html)
{
    std::vector<TocEntry> entries;
    size_t pos = 0;
    while (pos < html.size())
    {
        // Find <h2 or <h3
        size_t h = html.find("<h", pos);
        if (h == std::string::npos) break;
        if (h + 3 >= html.size()) break;
        char level_ch = html[h + 2];
        if (level_ch != '2' && level_ch != '3') { pos = h + 3; continue; }
        int level = level_ch - '0';

        // Extract id="..."
        size_t id_start = html.find("id=\"", h);
        size_t tag_end = html.find('>', h);
        if (id_start == std::string::npos || tag_end == std::string::npos || id_start > tag_end)
        { pos = h + 3; continue; }
        id_start += 4;
        size_t id_end = html.find('"', id_start);
        if (id_end == std::string::npos) { pos = h + 3; continue; }
        std::string id = html.substr(id_start, id_end - id_start);

        // Extract text content (strip inner tags)
        size_t content_start = tag_end + 1;
        std::string close_tag = "</h" + std::string(1, level_ch) + ">";
        size_t content_end = html.find(close_tag, content_start);
        if (content_end == std::string::npos) { pos = h + 3; continue; }

        std::string text;
        bool in_tag = false;
        for (size_t i = content_start; i < content_end; ++i)
        {
            if (html[i] == '<') { in_tag = true; continue; }
            if (html[i] == '>') { in_tag = false; continue; }
            if (!in_tag) text += html[i];
        }

        if (!id.empty() && !text.empty())
            entries.push_back({level, std::move(id), std::move(text)});

        pos = content_end + close_tag.size();
    }
    return entries;
}

// ═══════════════════════════════════════════════════════════════════════
//  Static assets
// ═══════════════════════════════════════════════════════════════════════

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
//  Layout component defaults
// ═══════════════════════════════════════════════════════════════════════

// ── Document ──

Node Document::render(const Document& props, const Ctx& ctx, Children ch)
{
    return dom::fragment(
        dom::doctype(),
        html(lang("en"),
            ctx(Head{.page_meta = props.page_meta}),
            ctx(Body{}, std::move(ch))
        )
    );
}

// ── Head ──

static Node build_structured_data(const Site& site, const PageMeta& pm,
                                    const std::string& canonical, const std::string& og_image_url)
{
    auto esc = escape_json;
    std::vector<Node> nodes;

    if (pm.og_type == "article")
    {
        std::string j = "{\"@context\":\"https://schema.org\",\"@type\":\"BlogPosting\",";
        j += "\"headline\":\"" + esc(pm.title) + "\",";
        if (!pm.published_date.empty()) j += "\"datePublished\":\"" + pm.published_date + "\",";
        if (!site.author.empty()) j += "\"author\":{\"@type\":\"Person\",\"name\":\"" + esc(site.author) + "\"},";
        if (!site.base_url.empty()) j += "\"url\":\"" + esc(canonical) + "\",";
        if (!og_image_url.empty()) j += "\"image\":\"" + esc(og_image_url) + "\",";
        j += "\"publisher\":{\"@type\":\"Organization\",\"name\":\"" + esc(site.title) + "\"}}";
        nodes.push_back(script(attr("type", "application/ld+json"), raw(j)));
    }
    else if (pm.canonical_path.empty() || pm.canonical_path == "/")
    {
        std::string j = "{\"@context\":\"https://schema.org\",\"@type\":\"WebSite\",";
        j += "\"name\":\"" + esc(site.title) + "\",";
        if (!site.base_url.empty()) j += "\"url\":\"" + site.base_url + "\",";
        j += "\"description\":\"" + esc(site.description) + "\"}";
        nodes.push_back(script(attr("type", "application/ld+json"), raw(j)));
    }

    struct Crumb { std::string name, path; };
    std::vector<Crumb> crumbs;
    const auto& cp = pm.canonical_path;
    if (pm.og_type == "article") crumbs = {{"Home", "/"}, {pm.title, cp}};
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
        nodes.push_back(script(attr("type", "application/ld+json"), raw(j)));
    }

    Node f;
    f.kind = Node::Fragment;
    f.children = std::move(nodes);
    return f;
}

Node Head::render(const Head& props, const Ctx& ctx, Children)
{
    const auto& site   = ctx.site;
    const auto& layout = site.layout;
    const auto  pm     = props.page_meta ? *props.page_meta : PageMeta{};

    std::string og_image_url;
    if (!pm.og_image.empty())
    {
        if (pm.og_image.find("://") != std::string::npos) og_image_url = pm.og_image;
        else if (!site.base_url.empty()) og_image_url = site.base_url + (pm.og_image.front() == '/' ? pm.og_image : "/" + pm.og_image);
        else og_image_url = pm.og_image;
    }

    std::string page_title = pm.title.empty() ? site.title : pm.title + " \xe2\x80\x94 " + site.title;
    std::string page_desc  = pm.description.empty() ? site.description : pm.description;
    std::string canonical  = site.base_url + pm.canonical_path;
    std::string og_title   = pm.title.empty() ? site.title : pm.title;

    // Theme CSS
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

    // Use dom:: prefix for meta/raw to avoid collision with struct fields
    return head(
        dom::meta(attr("charset", "utf-8")),
        dom::meta(name("viewport"), attr("content", "width=device-width, initial-scale=1")),
        title(page_title),
        dom::meta(name("description"), attr("content", page_desc)),
        when(pm.noindex, dom::meta(name("robots"), attr("content", "noindex, nofollow"))),
        when(!site.author.empty(), dom::meta(name("author"), attr("content", site.author))),
        when(!site.base_url.empty(), link(rel("canonical"), href(canonical))),
        when(pm.og_type == "article" && !og_image_url.empty(),
            link(rel("preload"), attr("as", "image"), href(og_image_url))),
        dom::meta(attr("property", "og:type"), attr("content", pm.og_type.empty() ? "website" : pm.og_type)),
        dom::meta(attr("property", "og:title"), attr("content", og_title)),
        dom::meta(attr("property", "og:description"), attr("content", page_desc)),
        when(!site.base_url.empty(), dom::fragment(
            dom::meta(attr("property", "og:url"), attr("content", canonical)),
            dom::meta(attr("property", "og:site_name"), attr("content", site.title)))),
        when(!site.author.empty() && pm.og_type == "article",
            dom::meta(attr("property", "article:author"), attr("content", site.author))),
        when(!pm.published_date.empty(),
            dom::meta(attr("property", "article:published_time"), attr("content", pm.published_date))),
        each(pm.tags, [](const std::string& t) {
            return dom::meta(attr("property", "article:tag"), attr("content", t)); }),
        when(!og_image_url.empty(),
            dom::meta(attr("property", "og:image"), attr("content", og_image_url))),
        dom::meta(name("twitter:card"), attr("content", og_image_url.empty() ? "summary" : "summary_large_image")),
        dom::meta(name("twitter:title"), attr("content", og_title)),
        dom::meta(name("twitter:description"), attr("content", page_desc)),
        when(!og_image_url.empty(),
            dom::meta(name("twitter:image"), attr("content", og_image_url))),
        when(!site.base_url.empty(),
            link(rel("alternate"), type("application/rss+xml"),
                 attr("title", site.title + " RSS"), href(site.base_url + "/feed.xml"))),
        build_structured_data(site, pm, canonical, og_image_url),
        dom::meta(name("view-transition"), attr("content", "same-origin")),
        when(layout.show_theme_toggle, script(dom::raw(THEME_JS))),
        when(!layout.custom_head_html.empty(), dom::raw(layout.custom_head_html)),
        // External CSS: content-hashed, immutably cached
        when(ctx.assets != nullptr, [&]() -> Node {
            if (ctx.assets->css_url.empty()) return Node{Node::Fragment, {}, {}, {}, {}};
            return link(rel("stylesheet"), href(ctx.assets->css_url));
        }),
        // Fallback: inline CSS when no asset manifest (e.g. theme previews)
        when(ctx.assets == nullptr, [&]() -> Node {
            return dom::fragment(
                style(dom::raw(site.theme.css.empty() ? DEFAULT_CSS : site.theme.css)),
                when(!theme_css.empty(), style(dom::raw(theme_css))),
                when(!var_css.empty(), style(dom::raw(var_css))),
                when(!layout.custom_css.empty(), style(dom::raw(layout.custom_css))));
        })
    );
}

// ── Body ──

Node Body::render(const Body&, const Ctx& ctx, Children ch)
{
    const auto& layout = ctx.site.layout;
    return body(
        classes({
            {"has-sidebar",     false},  // Determined by page() helper
            {"sidebar-left",    layout.sidebar_position == "left"},
            {"header-centered", layout.header_style == "centered"},
            {"header-minimal",  layout.header_style == "minimal"}
        }),
        component::fragment(std::move(ch))
    );
}

// ── Header ──

Node Header::render(const Header&, const Ctx& ctx, Children ch)
{
    const auto& site   = ctx.site;
    const auto& layout = site.layout;

    return header(
        div(class_("container header-bar"),
            div(class_("header-left"),
                h1(a(href("/"), site.title)),
                when(layout.show_description && !site.description.empty(),
                    p_(class_("site-description"), site.description)),
                ctx(Nav{})),
            when(layout.show_theme_toggle,
                ctx(ThemeToggle{})),
            component::fragment(std::move(ch)))
    );
}

// ── Footer ──

Node Footer::render(const Footer&, const Ctx& ctx, Children ch)
{
    const auto& site = ctx.site;
    return footer(
        div(class_("container"),
            p_(raw(!site.footer.copyright.empty() ? site.footer.copyright : "Powered by Loom")),
            when(!site.footer.links.empty(),
                div(class_("footer-links"),
                    each(site.footer.links, [](const auto& lnk) {
                        return a(href(lnk.url), lnk.title);
                    }))),
            component::fragment(std::move(ch)))
    );
}

// ── Nav ──

Node Nav::render(const Nav&, const Ctx& ctx, Children)
{
    return nav(
        ul(
            each(ctx.site.navigation.items, [](const auto& item) {
                return li(a(href(item.url), item.title));
            }),
            li(a(class_("nav-search"), href("/search"), attr("title", "Search"), raw("&#x2315;")))
        )
    );
}

// ── ThemeToggle ──

Node ThemeToggle::render(const ThemeToggle&, const Ctx&, Children)
{
    return button(class_("theme-toggle"), id("themeToggle"),
                  onclick("toggleTheme()"), raw("\xF0\x9F\x8C\x99"));
}

// ── Breadcrumbs ──

Node Breadcrumbs::render(const Breadcrumbs& props, const Ctx& ctx, Children)
{
    if (!ctx.site.layout.show_breadcrumbs) return Node{Node::Fragment, {}, {}, {}, {}};

    struct Crumb { std::string label, url; };
    std::vector<Crumb> crumbs;
    const auto& cp = props.canonical_path;

    if (props.og_type == "article")        crumbs = {{"Home", "/"}, {props.page_title, ""}};
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

// ── ContentArea ──

Node ContentArea::render(const ContentArea& props, const Ctx&, Children ch)
{
    if (props.has_sidebar)
        return div(class_("container with-sidebar"),
            main_(component::fragment(std::move(ch))),
            props.sidebar_node);

    return div(class_("container"),
        component::fragment(std::move(ch)));
}

// ═══════════════════════════════════════════════════════════════════════
//  Sidebar component defaults
// ═══════════════════════════════════════════════════════════════════════

Node Widget::render(const Widget& props, const Ctx&, Children ch)
{
    return div(class_("widget"),
        h3(props.heading),
        component::fragment(std::move(ch))
    );
}

Node RecentPostsWidget::render(const RecentPostsWidget& props, const Ctx& ctx, Children)
{
    if (!props.data) return Node{Node::Fragment, {}, {}, {}, {}};
    int count = ctx.site.sidebar.recent_posts_count;
    int i = 0;
    return ctx(Widget{.heading = "Recent Posts"},
        ul(each(props.data->recent_posts, [&](const PostSummary& post) -> Node {
            if (i++ >= count) return Node{Node::Fragment, {}, {}, {}, {}};
            return li(
                a(href("/post/" + post.slug.get()), post.title.get()),
                span(class_("date"),
                    raw(" " + format_date(post.published, resolve_date_format(ctx)))));
        }))
    );
}

Node TagCloudWidget::render(const TagCloudWidget& props, const Ctx& ctx, Children)
{
    if (!props.data) return Node{Node::Fragment, {}, {}, {}, {}};
    return ctx(Widget{.heading = "Tags"},
        div(class_("post-tags tag-cloud"),
            each(props.data->tag_infos, [](const TagInfo& ti) {
                auto size = 0.8f + ti.weight * 0.8f;
                auto s = std::to_string(size);
                s.erase(s.find_last_not_of('0') + 1);
                if (s.back() == '.') s.pop_back();
                return a(class_("tag"), href("/tag/" + ti.tag.get()),
                    attr("style", "font-size:" + s + "em"),
                    ti.tag.get());
            }))
    );
}

Node ArchivesWidget::render(const ArchivesWidget&, const Ctx& ctx, Children)
{
    return ctx(Widget{.heading = "Archives"},
        p_(a(href("/archives"), "Browse all posts by year"))
    );
}

Node SeriesWidget::render(const SeriesWidget& props, const Ctx& ctx, Children)
{
    if (!props.data || props.data->series_infos.empty()) return Node{Node::Fragment, {}, {}, {}, {}};
    return ctx(Widget{.heading = "Series"},
        ul(each(props.data->series_infos, [](const SeriesInfo& si) {
            return li(
                a(href("/series/" + si.name.get()), si.name.get()),
                span(class_("series-count"), " (" + std::to_string(si.post_count) + ")"));
        }))
    );
}

Node AboutWidget::render(const AboutWidget&, const Ctx& ctx, Children)
{
    return ctx(Widget{.heading = "About"},
        p_(ctx.site.sidebar.about_text)
    );
}

Node Sidebar::render(const Sidebar& props, const Ctx& ctx, Children ch)
{
    if (ctx.site.sidebar.widgets.empty()) return Node{Node::Fragment, {}, {}, {}, {}};

    return aside(class_("sidebar"),
        each(ctx.site.sidebar.widgets, [&](const std::string& w) -> Node {
            if (w == "recent_posts") return ctx(RecentPostsWidget{.data = props.data});
            if (w == "tag_cloud")    return ctx(TagCloudWidget{.data = props.data});
            if (w == "archives")     return ctx(ArchivesWidget{});
            if (w == "series")       return ctx(SeriesWidget{.data = props.data});
            if (w == "about" && !ctx.site.sidebar.about_text.empty())
                return ctx(AboutWidget{});
            return Node{Node::Fragment, {}, {}, {}, {}};
        }),
        component::fragment(std::move(ch))
    );
}

// ═══════════════════════════════════════════════════════════════════════
//  Post component defaults
// ═══════════════════════════════════════════════════════════════════════

Node TagList::render(const TagList& props, const Ctx&, Children)
{
    if (!props.tags || props.tags->empty()) return Node{Node::Fragment, {}, {}, {}, {}};
    return div(class_("post-tags"),
        each(*props.tags, [](const Tag& t) {
            return a(class_("tag"), href("/tag/" + t.get()), t.get());
        })
    );
}

Node PostMeta::render(const PostMeta& props, const Ctx& ctx, Children)
{
    if (!props.post) return Node{Node::Fragment, {}, {}, {}, {}};
    const auto& layout = ctx.site.layout;
    const auto& post   = *props.post;
    const auto& dfmt   = resolve_date_format(ctx);

    return when(layout.show_post_dates || layout.show_reading_time,
        div(class_("post-meta"),
            when(layout.show_post_dates,
                time_(format_date(post.published, dfmt))),
            when(layout.show_reading_time && post.reading_time_minutes > 0,
                span(class_("reading-time"),
                    std::to_string(post.reading_time_minutes) + " min read"))));
}

Node PostContent::render(const PostContent& props, const Ctx&, Children)
{
    if (!props.post) return Node{Node::Fragment, {}, {}, {}, {}};
    return div(class_("post-content"), raw(props.post->content.get()));
}

Node PostNav::render(const PostNav& props, const Ctx&, Children)
{
    if (!props.navigation) return Node{Node::Fragment, {}, {}, {}, {}};
    if (!props.navigation->prev && !props.navigation->next) return Node{Node::Fragment, {}, {}, {}, {}};

    return dom::nav(class_("post-nav"),
        props.navigation->prev
            ? a(class_("post-nav-prev"), href("/post/" + props.navigation->prev->slug.get()),
                raw("&larr; " + props.navigation->prev->title.get()))
            : span(),
        props.navigation->next
            ? a(class_("post-nav-next"), href("/post/" + props.navigation->next->slug.get()),
                raw(props.navigation->next->title.get() + " &rarr;"))
            : Node{Node::Fragment, {}, {}, {}, {}});
}

Node RelatedPosts::render(const RelatedPosts& props, const Ctx& ctx, Children)
{
    if (!props.posts || props.posts->empty()) return Node{Node::Fragment, {}, {}, {}, {}};
    const auto& layout = ctx.site.layout;

    return section(class_("related-posts"),
        h2("Related Posts"),
        ul(each(*props.posts, [&](const PostSummary& rp) {
            return li(
                a(href("/post/" + rp.slug.get()), rp.title.get()),
                when(layout.show_post_dates,
                    span(class_("date"),
                        raw(" " + format_date(rp.published, resolve_date_format(ctx))))));
        }))
    );
}

Node SeriesNav::render(const SeriesNav& props, const Ctx&, Children)
{
    if (props.series_name.empty() || !props.posts || props.posts->size() <= 1)
        return Node{Node::Fragment, {}, {}, {}, {}};

    int current_part = 0;
    int total = static_cast<int>(props.posts->size());
    for (int i = 0; i < total; ++i)
        if ((*props.posts)[i].slug.get() == props.current_slug)
        { current_part = i + 1; break; }

    std::string label = "Part " + std::to_string(current_part) + " of "
                      + std::to_string(total) + " in <strong>"
                      + props.series_name + "</strong>";

    return dom::details(class_("series-nav"),
        dom::summary(class_("series-label"), raw(label)),
        ol(class_("series-list"),
            each(*props.posts, [&](const PostSummary& sp) -> Node {
                if (sp.slug.get() == props.current_slug)
                    return li(class_("series-current"), sp.title.get());
                return li(a(href("/post/" + sp.slug.get()), sp.title.get()));
            }))
    );
}

Node PostFull::render(const PostFull& props, const Ctx& ctx, Children ch)
{
    if (!props.post) return Node{Node::Fragment, {}, {}, {}, {}};
    const auto& post   = *props.post;
    const auto& layout = ctx.site.layout;
    const auto  pctx   = props.context ? *props.context : PostContext{};

    // Staleness notice — show if post is older than 18 months
    auto age = std::chrono::system_clock::now() - post.published;
    auto days = std::chrono::duration_cast<std::chrono::hours>(age).count() / 24;
    int years = static_cast<int>(days / 365);
    bool is_stale = days > 548; // ~18 months
    std::string stale_text;
    if (is_stale)
        stale_text = years >= 2
            ? "This post was written over " + std::to_string(years) + " years ago. Some information may be outdated."
            : "This post was written over a year ago. Some information may be outdated.";

    return article(class_("post-full"),
        ctx(ReadingProgress{}),
        when(is_stale,
            div(class_("staleness-notice"), stale_text)),
        h1(post.title.get()),
        ctx(PostMeta{.post = props.post}),
        when(layout.show_post_tags && !post.tags.empty(),
            div(class_("post-full post-tags"),
                ctx(TagList{.tags = &post.tags}))),
        ctx(SeriesNav{
            .series_name = pctx.series_name.get(),
            .posts       = &pctx.series_posts,
            .current_slug = post.slug.get()
        }),
        ctx(TableOfContents{.entries = &pctx.toc}),
        ctx(PostContent{.post = props.post}),
        ctx(PostNav{.navigation = &pctx.nav}),
        ctx(RelatedPosts{.posts = &pctx.related}),
        component::fragment(std::move(ch))
    );
}

Node PostListing::render(const PostListing& props, const Ctx& ctx, Children)
{
    if (!props.post) return Node{Node::Fragment, {}, {}, {}, {}};
    const auto& post   = *props.post;
    const auto& layout = ctx.site.layout;
    const auto& dfmt   = resolve_date_format(ctx);

    return article(class_("post-listing"),
        a(href("/post/" + post.slug.get()), post.title.get()),
        when(layout.show_post_dates || (layout.show_reading_time && post.reading_time_minutes > 0),
            div(class_("post-listing-meta"),
                when(layout.show_post_dates,
                    span(class_("date"), format_date(post.published, dfmt))),
                when(layout.show_reading_time && post.reading_time_minutes > 0,
                    span(class_("reading-time"),
                        std::to_string(post.reading_time_minutes) + " min")))),
        when(layout.show_excerpts && !post.excerpt.empty(),
            p_(class_("excerpt"), post.excerpt)),
        when(layout.show_post_tags && !post.tags.empty(),
            ctx(TagList{.tags = &post.tags}))
    );
}

Node PostCard::render(const PostCard& props, const Ctx& ctx, Children)
{
    if (!props.post) return Node{Node::Fragment, {}, {}, {}, {}};
    const auto& post   = *props.post;
    const auto& layout = ctx.site.layout;
    const auto& dfmt   = resolve_date_format(ctx);

    return div(class_("post-card"),
        a(href("/post/" + post.slug.get()), post.title.get()),
        when(layout.show_post_dates,
            span(class_("date"), format_date(post.published, dfmt))),
        when(layout.show_reading_time && post.reading_time_minutes > 0,
            span(class_("reading-time"),
                std::to_string(post.reading_time_minutes) + " min")),
        when(layout.show_excerpts && !post.excerpt.empty(),
            p_(class_("excerpt"), post.excerpt)),
        when(layout.show_post_tags && !post.tags.empty(),
            ctx(TagList{.tags = &post.tags}))
    );
}

// ═══════════════════════════════════════════════════════════════════════
//  Page-level component defaults
// ═══════════════════════════════════════════════════════════════════════

Node FeaturedSection::render(const FeaturedSection& props, const Ctx& ctx, Children)
{
    if (!props.posts || props.posts->empty()) return Node{Node::Fragment, {}, {}, {}, {}};
    return section(class_("featured-posts"),
        h2("Featured"),
        div(class_("post-cards"),
            each(*props.posts, [&](const PostSummary& p) {
                return ctx(PostCard{.post = &p});
            }))
    );
}

Node Pagination::render(const Pagination& props, const Ctx&, Children)
{
    const auto& info = props.info;
    if (info.total_pages <= 1) return Node{Node::Fragment, {}, {}, {}, {}};

    Children page_links;
    for (int i = 1; i <= info.total_pages; ++i)
    {
        std::string url = (i == 1) ? "/" : "/page/" + std::to_string(i);
        if (i == info.current_page)
            page_links.push_back(span(class_("page-num current"), std::to_string(i)));
        else
            page_links.push_back(a(class_("page-num"), href(url), std::to_string(i)));
    }

    return nav(class_("pagination"), aria_label("Pagination"),
        when(info.has_prev(),
            a(class_("page-prev"), href(info.prev_url()), raw("&larr; Prev"))),
        div(class_("page-numbers"), component::fragment(std::move(page_links))),
        when(info.has_next(),
            a(class_("page-next"), href(info.next_url()), raw("Next &rarr;"))),
        span(class_("page-info"),
            "Page " + std::to_string(info.current_page) + " of " + std::to_string(info.total_pages))
    );
}

Node Index::render(const Index& props, const Ctx& ctx, Children)
{
    if (!props.posts) return Node{Node::Fragment, {}, {}, {}, {}};
    const auto& layout = ctx.site.layout;
    const char* heading = (ctx.theme_def && !ctx.theme_def->index_heading.empty())
                              ? ctx.theme_def->index_heading.c_str()
                              : "Recent Posts";

    auto featured_node = (props.featured && !props.featured->empty())
        ? ctx(FeaturedSection{.posts = props.featured})
        : Node{Node::Fragment, {}, {}, {}, {}};

    auto pagination_node = props.pagination
        ? ctx(Pagination{.info = *props.pagination})
        : Node{Node::Fragment, {}, {}, {}, {}};

    if (layout.post_list_style == "cards")
    {
        return section(
            featured_node,
            h2(heading),
            div(class_("post-cards"),
                each(*props.posts, [&](const PostSummary& p) {
                    return ctx(PostCard{.post = &p});
                })),
            pagination_node
        );
    }

    return section(
        featured_node,
        h2(heading),
        each(*props.posts, [&](const PostSummary& p) {
            return ctx(PostListing{.post = &p});
        }),
        pagination_node
    );
}

Node TagPage::render(const TagPage& props, const Ctx& ctx, Children)
{
    if (!props.posts) return Node{Node::Fragment, {}, {}, {}, {}};
    return section(
        h2(raw("Posts tagged &ldquo;" + props.tag.get() + "&rdquo; (" + std::to_string(props.post_count) + ")")),
        each(*props.posts, [&](const PostSummary& p) {
            return ctx(PostListing{.post = &p});
        })
    );
}

Node TagIndex::render(const TagIndex& props, const Ctx&, Children)
{
    if (!props.tag_infos) return Node{Node::Fragment, {}, {}, {}, {}};
    return section(
        h2("Tags"),
        div(class_("post-tags tag-cloud"),
            each(*props.tag_infos, [](const TagInfo& ti) {
                auto size = 0.8f + ti.weight * 0.8f;
                auto s = std::to_string(size);
                s.erase(s.find_last_not_of('0') + 1);
                if (s.back() == '.') s.pop_back();
                return a(class_("tag"), href("/tag/" + ti.tag.get()),
                    attr("style", "font-size:" + s + "em"),
                    ti.tag.get() + " (" + std::to_string(ti.count) + ")");
            }))
    );
}

Node Archives::render(const Archives& props, const Ctx& ctx, Children)
{
    if (!props.by_year) return Node{Node::Fragment, {}, {}, {}, {}};
    const auto& layout = ctx.site.layout;

    // Collect all posts for the graph
    std::vector<PostSummary> all_for_graph;
    for (const auto& [year, posts] : *props.by_year)
        for (const auto& p : posts) all_for_graph.push_back(p);

    return section(
        h2("Archives"),
        ctx(PostGraph{.posts = &all_for_graph}),
        each(*props.by_year, [&](const auto& pair) {
            return dom::fragment(
                h3(std::to_string(pair.first)),
                each(pair.second, [&](const PostSummary& p) {
                    return article(class_("post-listing"),
                        when(layout.show_post_dates,
                            span(class_("date"), format_date(p.published, resolve_date_format(ctx)))),
                        raw(" "),
                        a(href("/post/" + p.slug.get()), p.title.get()));
                }));
        })
    );
}

Node SeriesPage::render(const SeriesPage& props, const Ctx& ctx, Children)
{
    if (!props.posts) return Node{Node::Fragment, {}, {}, {}, {}};
    const auto& layout = ctx.site.layout;

    return section(
        h2(raw("Series: " + props.series.get())),
        ol(class_("series-list"),
            each(*props.posts, [&](const PostSummary& p) {
                return li(
                    a(href("/post/" + p.slug.get()), p.title.get()),
                    when(layout.show_post_dates,
                        span(class_("date"),
                            raw(" " + format_date(p.published, resolve_date_format(ctx))))));
            }))
    );
}

Node SeriesIndex::render(const SeriesIndex& props, const Ctx& ctx, Children)
{
    if (!props.all_series_info) return Node{Node::Fragment, {}, {}, {}, {}};
    const auto& dfmt = resolve_date_format(ctx);

    return section(
        h2("Series"),
        each(*props.all_series_info, [&](const SeriesInfo& si) {
            return article(class_("series-card"),
                h3(a(href("/series/" + si.name.get()), si.name.get())),
                div(class_("series-meta"),
                    span(std::to_string(si.post_count) + (si.post_count == 1 ? " post" : " posts")),
                    span(class_("date"),
                        format_date(si.first_date, dfmt) + " \xe2\x80\x93 " + format_date(si.last_date, dfmt))),
                when(!si.latest_post_title.get().empty(),
                    p_(class_("series-latest"),
                        raw("Latest: " + si.latest_post_title.get())))
            );
        })
    );
}

Node PageView::render(const PageView& props, const Ctx& ctx, Children ch)
{
    if (!props.page) return Node{Node::Fragment, {}, {}, {}, {}};
    return article(class_("page"),
        ctx(ReadingProgress{}),
        h1(props.page->title.get()),
        ctx(TableOfContents{.entries = &props.toc}),
        div(class_("page-content"), raw(props.page->content.get())),
        component::fragment(std::move(ch))
    );
}

Node NotFound::render(const NotFound&, const Ctx&, Children)
{
    return section(
        h2("404 \xe2\x80\x94 Not Found"),
        p_("The page you're looking for doesn't exist.")
    );
}

static const char* SEARCH_JS = R"JS(
(function() {
    var data = null;
    var input = document.getElementById('searchInput');
    var results = document.getElementById('searchResults');
    if (!input || !results) return;

    fetch('/search.json')
        .then(function(r) { return r.json(); })
        .then(function(d) { data = d; });

    input.addEventListener('input', function() {
        if (!data) return;
        var q = input.value.toLowerCase().trim();
        if (q.length < 2) { results.innerHTML = ''; return; }

        var terms = q.split(/\s+/);
        var matches = data.filter(function(p) {
            var haystack = (p.t + ' ' + p.e + ' ' + p.g.join(' ')).toLowerCase();
            return terms.every(function(t) { return haystack.indexOf(t) !== -1; });
        });

        if (matches.length === 0) {
            results.innerHTML = '<p class="search-empty">No results found.</p>';
            return;
        }

        var html = '';
        var esc = function(s) {
            var d = document.createElement('div');
            d.textContent = s;
            return d.innerHTML;
        };
        matches.forEach(function(p) {
            html += '<article class="post-listing"><a href="/post/' + esc(p.s) + '">' + esc(p.t) + '</a>';
            if (p.e) html += '<p class="excerpt">' + esc(p.e).substring(0, 150) + '</p>';
            html += '</article>';
        });
        results.innerHTML = html;
    });
})();
)JS";

Node SearchPage::render(const SearchPage&, const Ctx&, Children)
{
    return section(class_("search-page"),
        h2("Search"),
        div(class_("search-box"),
            input(attr("type", "text"), id("searchInput"),
                attr("placeholder", "Search posts..."),
                attr("autocomplete", "off"),
                attr("autofocus", ""))),
        div(id("searchResults")),
        script(raw(SEARCH_JS))
    );
}

Node TableOfContents::render(const TableOfContents& props, const Ctx&, Children)
{
    if (!props.entries || props.entries->size() < 3)
        return Node{Node::Fragment, {}, {}, {}, {}};

    return dom::details(class_("toc"),
        dom::summary(class_("toc-title"),
            "Contents (" + std::to_string(props.entries->size()) + ")"),
        ul(class_("toc-list"),
            each(*props.entries, [](const TocEntry& e) {
                return li(class_(e.level == 3 ? "toc-h3" : "toc-h2"),
                    a(href("#" + e.id), e.text));
            }))
    );
}

static const char* PROGRESS_JS = R"JS(
(function(){
    var bar = document.getElementById('readingProgress');
    if (!bar) return;
    var article = document.querySelector('.post-content,.page-content');
    if (!article) return;
    function update() {
        var rect = article.getBoundingClientRect();
        var total = rect.height - window.innerHeight;
        if (total <= 0) { bar.style.width = '100%'; return; }
        var pct = Math.min(100, Math.max(0, (-rect.top / total) * 100));
        bar.style.width = pct + '%';
    }
    window.addEventListener('scroll', update, {passive: true});
    update();
})();
)JS";

Node ReadingProgress::render(const ReadingProgress&, const Ctx&, Children)
{
    return dom::fragment(
        div(class_("reading-progress-bar"),
            div(class_("reading-progress-fill"), id("readingProgress"))),
        script(raw(PROGRESS_JS))
    );
}

// ═══════════════════════════════════════════════════════════════════════
//  UX Enhancement JavaScript
// ═══════════════════════════════════════════════════════════════════════

static const char* CMD_PALETTE_JS = R"JS(
(function(){
  var d=null,overlay=null,input=null,list=null,sel=0,matches=[];
  function esc(s){var d=document.createElement('div');d.textContent=s;return d.innerHTML}
  function create(){
    if(overlay)return;
    overlay=document.createElement('div');
    overlay.className='cmd-palette-overlay';
    overlay.innerHTML='<div class="cmd-palette"><input type="text" class="cmd-input" placeholder="Navigate to..." autocomplete="off"><div class="cmd-results"></div></div>';
    document.body.appendChild(overlay);
    input=overlay.querySelector('.cmd-input');
    list=overlay.querySelector('.cmd-results');
    overlay.addEventListener('click',function(e){if(e.target===overlay)close()});
    input.addEventListener('input',search);
    input.addEventListener('keydown',function(e){
      if(e.key==='ArrowDown'){e.preventDefault();sel=Math.min(sel+1,matches.length-1);highlight()}
      else if(e.key==='ArrowUp'){e.preventDefault();sel=Math.max(sel-1,0);highlight()}
      else if(e.key==='Enter'&&matches[sel]){close();window.location='/post/'+matches[sel].s}
      else if(e.key==='Escape'){close()}
    });
  }
  function open(){
    create();
    overlay.classList.add('visible');
    input.value='';list.innerHTML='';sel=0;matches=[];
    setTimeout(function(){input.focus()},10);
    if(!d)fetch('/search.json').then(function(r){return r.json()}).then(function(j){d=j});
  }
  function close(){if(overlay)overlay.classList.remove('visible')}
  function highlight(){
    var items=list.querySelectorAll('.cmd-item');
    items.forEach(function(el,i){el.classList.toggle('active',i===sel)});
  }
  function search(){
    if(!d)return;
    var q=input.value.toLowerCase().trim();
    if(q.length<1){list.innerHTML='';matches=[];return}
    var terms=q.split(/\s+/);
    matches=d.filter(function(p){
      var h=(p.t+' '+p.e+' '+p.g.join(' ')).toLowerCase();
      return terms.every(function(t){return h.indexOf(t)!==-1})
    }).slice(0,8);
    sel=0;
    if(!matches.length){list.innerHTML='<div class="cmd-empty">No results</div>';return}
    var h='';
    matches.forEach(function(p,i){
      h+='<a href="/post/'+esc(p.s)+'" class="cmd-item'+(i===0?' active':'')+'">';
      h+='<span class="cmd-title">'+esc(p.t)+'</span>';
      if(p.g.length)h+='<span class="cmd-tags">'+p.g.map(esc).join(', ')+'</span>';
      h+='</a>'
    });
    list.innerHTML=h;
    list.querySelectorAll('.cmd-item').forEach(function(el,i){
      el.addEventListener('mouseenter',function(){sel=i;highlight()})
    });
  }
  document.addEventListener('keydown',function(e){
    if((e.metaKey||e.ctrlKey)&&e.key==='k'){e.preventDefault();open()}
    if(e.key==='Escape')close();
  });
})();
)JS";

static const char* KEYBOARD_NAV_JS = R"JS(
(function(){
  document.addEventListener('keydown',function(e){
    if(e.target.tagName==='INPUT'||e.target.tagName==='TEXTAREA'||e.target.isContentEditable)return;
    var items=document.querySelectorAll('.post-listing,.post-card');
    if(!items.length)return;
    var cur=document.querySelector('.post-listing.kb-focus,.post-card.kb-focus');
    var idx=cur?Array.prototype.indexOf.call(items,cur):-1;
    if(e.key==='j'){e.preventDefault();idx=Math.min(idx+1,items.length-1);setFocus(items,idx)}
    else if(e.key==='k'){e.preventDefault();idx=Math.max(idx-1,0);setFocus(items,idx)}
    else if(e.key==='Enter'&&cur){var a=cur.querySelector('a');if(a)a.click()}
    else if(e.key==='/'){
      var si=document.getElementById('searchInput');
      if(si){e.preventDefault();si.focus()}
    }
  });
  function setFocus(items,idx){
    items.forEach(function(el){el.classList.remove('kb-focus')});
    if(items[idx]){items[idx].classList.add('kb-focus');items[idx].scrollIntoView({block:'nearest',behavior:'smooth'})}
  }
})();
)JS";

static const char* ACTIVE_TOC_JS = R"JS(
(function(){
  var toc=document.querySelector('.toc');
  if(!toc)return;
  var links=toc.querySelectorAll('.toc-list a');
  if(!links.length)return;
  var ids=[];
  links.forEach(function(a){var h=a.getAttribute('href');if(h)ids.push(h.slice(1))});
  var headings=ids.map(function(id){return document.getElementById(id)}).filter(Boolean);
  if(!headings.length)return;
  var observer=new IntersectionObserver(function(entries){
    entries.forEach(function(entry){
      if(entry.isIntersecting){
        links.forEach(function(a){a.classList.remove('toc-active')});
        var match=toc.querySelector('a[href="#'+entry.target.id+'"]');
        if(match)match.classList.add('toc-active');
      }
    });
  },{rootMargin:'-80px 0px -70% 0px'});
  headings.forEach(function(h){observer.observe(h)});
})();
)JS";

static const char* IMAGE_ZOOM_JS = R"JS(
(function(){
  var content=document.querySelector('.post-content,.page-content');
  if(!content)return;
  content.querySelectorAll('img').forEach(function(img){
    img.style.cursor='zoom-in';
    img.addEventListener('click',function(){
      var overlay=document.createElement('div');
      overlay.className='img-zoom-overlay';
      var clone=img.cloneNode();
      clone.className='img-zoom-full';
      clone.removeAttribute('loading');
      overlay.appendChild(clone);
      document.body.appendChild(overlay);
      requestAnimationFrame(function(){overlay.classList.add('visible')});
      function close(){overlay.classList.remove('visible');setTimeout(function(){overlay.remove()},200)}
      overlay.addEventListener('click',close);
      document.addEventListener('keydown',function handler(e){
        if(e.key==='Escape'){close();document.removeEventListener('keydown',handler)}
      });
    });
  });
})();
)JS";

static const char* READING_POS_JS = R"JS(
(function(){
  var article=document.querySelector('.post-content');
  if(!article)return;
  var slug=window.location.pathname;
  var key='loom-pos:'+slug;
  var saved=localStorage.getItem(key);
  if(saved&&parseInt(saved)>300){
    var toast=document.createElement('div');
    toast.className='reading-toast';
    toast.innerHTML='Continue where you left off? <a href="#" id="resumePos">Resume</a>';
    document.body.appendChild(toast);
    requestAnimationFrame(function(){toast.classList.add('visible')});
    document.getElementById('resumePos').addEventListener('click',function(e){
      e.preventDefault();window.scrollTo({top:parseInt(saved),behavior:'smooth'});
      toast.classList.remove('visible');setTimeout(function(){toast.remove()},300);
    });
    setTimeout(function(){toast.classList.remove('visible');setTimeout(function(){toast.remove()},300)},6000);
  }
  var ticking=false;
  window.addEventListener('scroll',function(){
    if(!ticking){ticking=true;requestAnimationFrame(function(){
      localStorage.setItem(key,String(window.scrollY));ticking=false;
    })}
  },{passive:true});
})();
)JS";

static const char* CODE_COPY_JS = R"JS(
(function(){
  document.querySelectorAll('.code-block').forEach(function(block){
    var btn=document.createElement('button');
    btn.className='code-copy';btn.textContent='Copy';btn.setAttribute('aria-label','Copy code');
    block.style.position='relative';
    block.appendChild(btn);
    btn.addEventListener('click',function(){
      var code=block.querySelector('code');
      if(code){navigator.clipboard.writeText(code.textContent).then(function(){
        btn.textContent='Copied!';setTimeout(function(){btn.textContent='Copy'},2000);
      })}
    });
  });
})();
)JS";

// ── PostGraph: interactive force-directed graph ──
//
// Server-side: force layout + MST edge selection → JSON data
// Client-side: SVG rendering, pan/zoom, hover highlighting, semantic labels

static const char* POSTGRAPH_JS = R"JS(
(function(){
var el=document.getElementById('post-graph-container');
if(!el)return;
var data=JSON.parse(document.getElementById('post-graph-data').textContent);
var N=data.nodes,E=data.edges;
var W=data.w,H=data.h;

var svg=document.createElementNS('http://www.w3.org/2000/svg','svg');
svg.setAttribute('viewBox','0 0 '+W+' '+H);
svg.setAttribute('class','post-graph');
el.appendChild(svg);

var hint=document.createElement('div');
hint.className='post-graph-hint';
hint.textContent='Click to explore \u2022 Hover nodes to see connections';
el.appendChild(hint);

var g=document.createElementNS('http://www.w3.org/2000/svg','g');
svg.appendChild(g);

// Tooltip: single HTML div positioned over the SVG, not SVG text.
// HTML text never scales with the viewBox — always readable.
var tip=document.createElement('div');
tip.className='post-graph-tip';
tip.style.display='none';
el.appendChild(tip);

var zoom=1,panX=0,panY=0,dragging=false,dragX=0,dragY=0;
var activeNode=-1,expanded=false;

var adj={};
E.forEach(function(e){
  if(!adj[e.a])adj[e.a]=[];
  if(!adj[e.b])adj[e.b]=[];
  adj[e.a].push(e.b);
  adj[e.b].push(e.a);
});

function updateTransform(){
  g.setAttribute('transform','translate('+panX+','+panY+') scale('+zoom+')');
}

// Convert SVG coords to container pixel coords
function svgToScreen(sx,sy){
  var rect=svg.getBoundingClientRect();
  var px=(sx*zoom+panX)/W*rect.width;
  var py=(sy*zoom+panY)/H*rect.height;
  return {x:px,y:py};
}

// ── Expand / Collapse ──

function expand(){
  if(expanded)return;
  expanded=true;
  hint.style.display='none';
  el.classList.add('expanded');
  var close=document.createElement('button');
  close.className='post-graph-close';
  close.innerHTML='&times;';
  close.title='Close (Esc)';
  close.addEventListener('click',function(ev){ev.stopPropagation();collapse()});
  el.appendChild(close);
  el._close=close;
  document.body.style.overflow='hidden';
}

function collapse(){
  if(!expanded)return;
  expanded=false;
  el.classList.remove('expanded');
  if(el._close){el._close.remove();el._close=null}
  zoom=1;panX=0;panY=0;
  updateTransform();
  document.body.style.overflow='';
  activeNode=-1;
  resetStyles();
  tip.style.display='none';
  hint.style.display='';
}

document.addEventListener('keydown',function(ev){
  if(ev.key==='Escape'&&expanded)collapse();
});

var firstClick=true;
function maybeExpand(){if(firstClick){firstClick=false;expand()}}

// ── Draw edges ──

var edgeEls=[];
E.forEach(function(e){
  var line=document.createElementNS('http://www.w3.org/2000/svg','line');
  line.setAttribute('x1',N[e.a].x);line.setAttribute('y1',N[e.a].y);
  line.setAttribute('x2',N[e.b].x);line.setAttribute('y2',N[e.b].y);
  line.setAttribute('stroke','var(--accent)');
  line.setAttribute('stroke-width',e.w>=4?'1.5':e.w>=3?'1':'0.6');
  line.setAttribute('stroke-opacity',e.w>=4?'0.3':e.w>=3?'0.18':'0.08');
  line.dataset.a=e.a;line.dataset.b=e.b;
  g.appendChild(line);edgeEls.push(line);
});

// ── Draw nodes (no SVG labels — use HTML tooltip instead) ──

var nodeEls=[];
N.forEach(function(nd,i){
  var c=document.createElementNS('http://www.w3.org/2000/svg','circle');
  c.setAttribute('cx',nd.x);c.setAttribute('cy',nd.y);
  c.setAttribute('r',nd.r);
  c.setAttribute('fill','var(--accent)');
  c.setAttribute('fill-opacity',nd.d>0?'0.65':'0.25');
  c.style.cursor='pointer';
  c.style.transition='fill-opacity 0.12s,r 0.12s';

  c.addEventListener('mouseenter',function(ev){
    maybeExpand();highlight(i);showTip(i,ev);
  });
  c.addEventListener('mousemove',function(ev){showTip(i,ev)});
  c.addEventListener('mouseleave',function(){unhighlight();tip.style.display='none'});
  c.addEventListener('click',function(ev){
    ev.preventDefault();
    if(!expanded){expand();highlight(i);return}
    window.location='/post/'+nd.s;
  });
  var lastTap=0;
  c.addEventListener('touchend',function(ev){
    ev.preventDefault();
    if(!expanded){expand();return}
    var now=Date.now();
    if(now-lastTap<300) window.location='/post/'+nd.s;
    else {highlight(i);showTipAt(i)}
    lastTap=now;
  });

  g.appendChild(c);
  nodeEls.push(c);
});

// ── Tooltip ──

function showTip(i,ev){
  var nd=N[i];
  var conn=adj[i]||[];
  var html='<strong>'+nd.t+'</strong>';
  if(conn.length>0){
    html+='<span class="post-graph-tip-conn">';
    html+=conn.length+' connection'+(conn.length>1?'s':'');
    html+='</span>';
  }
  tip.innerHTML=html;
  tip.style.display='block';
  // Position near cursor
  var rect=el.getBoundingClientRect();
  var tx=ev.clientX-rect.left+12;
  var ty=ev.clientY-rect.top-10;
  // Keep in bounds
  if(tx+200>rect.width) tx=ev.clientX-rect.left-212;
  if(ty<0) ty=ev.clientY-rect.top+20;
  tip.style.left=tx+'px';
  tip.style.top=ty+'px';
}

function showTipAt(i){
  var nd=N[i];
  var conn=adj[i]||[];
  var html='<strong>'+nd.t+'</strong>';
  if(conn.length>0){
    html+='<span class="post-graph-tip-conn">';
    html+=conn.length+' connection'+(conn.length>1?'s':'');
    html+='</span>';
  }
  tip.innerHTML=html;
  tip.style.display='block';
  var p=svgToScreen(nd.x,nd.y);
  tip.style.left=(p.x+15)+'px';
  tip.style.top=(p.y-10)+'px';
}

// ── Highlight: show hovered node + neighbors ──

function highlight(i){
  if(activeNode===i)return;
  activeNode=i;
  var connected=adj[i]||[];
  var connSet={};connSet[i]=1;
  connected.forEach(function(j){connSet[j]=1});

  nodeEls.forEach(function(c,j){
    if(j===i){
      c.setAttribute('fill-opacity','1');
      c.setAttribute('r',N[j].r*1.5);
    }else if(connSet[j]){
      c.setAttribute('fill-opacity','0.9');
    }else{
      c.setAttribute('fill-opacity','0.05');
    }
  });
  edgeEls.forEach(function(line){
    var a=+line.dataset.a,b=+line.dataset.b;
    var on=(a===i||b===i);
    line.setAttribute('stroke-opacity',on?'0.6':'0.015');
    line.setAttribute('stroke-width',on?'2':'0.3');
  });
}

function unhighlight(){
  if(activeNode<0)return;
  activeNode=-1;
  resetStyles();
}

function resetStyles(){
  nodeEls.forEach(function(c,j){
    c.setAttribute('fill-opacity',N[j].d>0?'0.65':'0.25');
    c.setAttribute('r',N[j].r);
  });
  edgeEls.forEach(function(line){
    var e=E.find(function(e){return e.a==+line.dataset.a&&e.b==+line.dataset.b});
    if(e){
      line.setAttribute('stroke-opacity',e.w>=4?'0.3':e.w>=3?'0.18':'0.08');
      line.setAttribute('stroke-width',e.w>=4?'1.5':e.w>=3?'1':'0.6');
    }
  });
}

// ── Pan / Zoom ──

svg.addEventListener('wheel',function(ev){
  ev.preventDefault();maybeExpand();
  var rect=svg.getBoundingClientRect();
  var mx=(ev.clientX-rect.left)/rect.width*W;
  var my=(ev.clientY-rect.top)/rect.height*H;
  var oldZ=zoom;
  zoom=Math.max(0.5,Math.min(8,zoom*(ev.deltaY>0?0.9:1.1)));
  panX=mx-(mx-panX)*(zoom/oldZ);
  panY=my-(my-panY)*(zoom/oldZ);
  updateTransform();
},{passive:false});

svg.addEventListener('mousedown',function(ev){
  if(ev.button!==0)return;maybeExpand();
  dragging=true;dragX=ev.clientX;dragY=ev.clientY;
  svg.style.cursor='grabbing';ev.preventDefault();
});
window.addEventListener('mousemove',function(ev){
  if(!dragging)return;
  var rect=svg.getBoundingClientRect();
  panX+=(ev.clientX-dragX)*(W/rect.width);
  panY+=(ev.clientY-dragY)*(H/rect.height);
  dragX=ev.clientX;dragY=ev.clientY;
  updateTransform();
});
window.addEventListener('mouseup',function(){
  if(!dragging)return;dragging=false;svg.style.cursor='grab';
});

var touches={};
svg.addEventListener('touchstart',function(ev){
  maybeExpand();
  for(var i=0;i<ev.changedTouches.length;i++){
    var t=ev.changedTouches[i];
    touches[t.identifier]={x:t.clientX,y:t.clientY};
  }
},{passive:true});
svg.addEventListener('touchmove',function(ev){
  ev.preventDefault();
  if(ev.touches.length===1){
    var t=ev.touches[0],prev=touches[t.identifier];
    if(prev){
      var rect=svg.getBoundingClientRect();
      panX+=(t.clientX-prev.x)*(W/rect.width);
      panY+=(t.clientY-prev.y)*(H/rect.height);
      touches[t.identifier]={x:t.clientX,y:t.clientY};
      updateTransform();
    }
  }else if(ev.touches.length===2){
    var t1=ev.touches[0],t2=ev.touches[1];
    var dist=Math.hypot(t1.clientX-t2.clientX,t1.clientY-t2.clientY);
    if(touches._pinch){
      zoom=Math.max(0.5,Math.min(8,zoom*dist/touches._pinch));
      updateTransform();
    }
    touches._pinch=dist;
    touches[t1.identifier]={x:t1.clientX,y:t1.clientY};
    touches[t2.identifier]={x:t2.clientX,y:t2.clientY};
  }
},{passive:false});
svg.addEventListener('touchend',function(ev){
  for(var i=0;i<ev.changedTouches.length;i++)
    delete touches[ev.changedTouches[i].identifier];
  if(ev.touches.length<2)delete touches._pinch;
},{passive:true});

svg.addEventListener('dblclick',function(ev){
  ev.preventDefault();
  if(!expanded){expand();return}
  zoom=1;panX=0;panY=0;updateTransform();
});

svg.style.cursor='grab';
updateTransform();
})();
)JS";

Node PostGraph::render(const PostGraph& props, const Ctx&, Children)
{
    if (!props.posts || props.posts->size() < 3)
        return Node{Node::Fragment, {}, {}, {}, {}};

    const auto& posts = *props.posts;
    auto n = std::min(posts.size(), size_t(30));
    constexpr double PI = 3.14159265358979;

    // ── Build adjacency ──
    struct Edge { size_t a, b; int weight; };
    std::vector<std::vector<std::pair<size_t, int>>> adj(n);

    for (size_t i = 0; i < n; ++i)
        for (size_t j = i + 1; j < n; ++j)
        {
            int shared = 0;
            for (const auto& ti : posts[i].tags)
                for (const auto& tj : posts[j].tags)
                    if (ti.get() == tj.get()) ++shared;
            if (shared >= 2)
            {
                adj[i].push_back({j, shared});
                adj[j].push_back({i, shared});
            }
        }

    // MST + strong extras for sparse graph
    std::vector<Edge> all_edges;
    for (size_t i = 0; i < n; ++i)
        for (const auto& [j, w] : adj[i])
            if (i < j) all_edges.push_back({i, j, w});
    std::sort(all_edges.begin(), all_edges.end(),
        [](const Edge& a, const Edge& b) { return a.weight > b.weight; });

    std::vector<size_t> parent(n);
    for (size_t i = 0; i < n; ++i) parent[i] = i;
    std::function<size_t(size_t)> find = [&](size_t x) -> size_t {
        return parent[x] == x ? x : (parent[x] = find(parent[x]));
    };

    std::vector<Edge> edges;
    size_t max_edges = std::min(n + n / 3, all_edges.size());
    for (const auto& e : all_edges)
    {
        if (edges.size() >= max_edges) break;
        size_t ra = find(e.a), rb = find(e.b);
        if (ra != rb) { parent[ra] = rb; edges.push_back(e); }
        else if (edges.size() < max_edges && e.weight >= 3) edges.push_back(e);
    }

    std::vector<int> degree(n, 0);
    for (const auto& e : edges) { degree[e.a] += e.weight; degree[e.b] += e.weight; }

    // ── Force-directed layout (unconstrained → normalize) ──
    //
    // Key insight: DO NOT clamp positions during simulation. Let the
    // physics determine the natural layout, then scale to fit the canvas.
    // Clamping during simulation creates artificial clusters at boundaries.

    struct FNode { double x, y, vx, vy; };
    std::vector<FNode> pos(n);
    for (size_t i = 0; i < n; ++i)
    {
        double angle = (2.0 * PI * i) / n;
        pos[i] = { 200.0 * std::cos(angle), 200.0 * std::sin(angle), 0, 0 };
    }

    for (int iter = 0; iter < 200; ++iter)
    {
        double temp = 2.0 * (1.0 - (double)iter / 200.0);

        // Repulsion (Coulomb)
        for (size_t i = 0; i < n; ++i)
            for (size_t j = i + 1; j < n; ++j)
            {
                double dx = pos[i].x - pos[j].x, dy = pos[i].y - pos[j].y;
                double d2 = dx * dx + dy * dy;
                if (d2 < 1) d2 = 1;
                double d = std::sqrt(d2);
                double f = 30000.0 / d2;
                double fx = dx / d * f * temp, fy = dy / d * f * temp;
                pos[i].vx += fx; pos[i].vy += fy;
                pos[j].vx -= fx; pos[j].vy -= fy;
            }

        // Attraction along edges (spring to ideal length)
        for (const auto& e : edges)
        {
            double dx = pos[e.b].x - pos[e.a].x, dy = pos[e.b].y - pos[e.a].y;
            double d = std::sqrt(dx * dx + dy * dy);
            if (d < 1) d = 1;
            double ideal = 100.0;
            double f = (d - ideal) * 0.02 * temp;
            double fx = dx / d * f, fy = dy / d * f;
            pos[e.a].vx += fx; pos[e.a].vy += fy;
            pos[e.b].vx -= fx; pos[e.b].vy -= fy;
        }

        // Gentle gravity toward origin
        for (size_t i = 0; i < n; ++i)
        {
            pos[i].vx -= pos[i].x * 0.002 * temp;
            pos[i].vy -= pos[i].y * 0.002 * temp;
            pos[i].x += pos[i].vx * 0.3;
            pos[i].y += pos[i].vy * 0.3;
            pos[i].vx *= 0.4;
            pos[i].vy *= 0.4;
            // NO clamping — let the layout breathe
        }
    }

    // Normalize: compute bounding box → scale to fit canvas with padding
    int w = 960, h = 500;
    double pad = 60.0;

    double minx = pos[0].x, maxx = pos[0].x, miny = pos[0].y, maxy = pos[0].y;
    for (size_t i = 1; i < n; ++i)
    {
        minx = std::min(minx, pos[i].x); maxx = std::max(maxx, pos[i].x);
        miny = std::min(miny, pos[i].y); maxy = std::max(maxy, pos[i].y);
    }

    double rangex = maxx - minx, rangey = maxy - miny;
    if (rangex < 1) rangex = 1;
    if (rangey < 1) rangey = 1;
    double scale = std::min((w - 2 * pad) / rangex, (h - 2 * pad) / rangey);

    for (size_t i = 0; i < n; ++i)
    {
        pos[i].x = pad + (pos[i].x - minx) * scale;
        pos[i].y = pad + (pos[i].y - miny) * scale;
    }

    // Center the layout in the canvas
    double cx2 = 0, cy2 = 0;
    for (size_t i = 0; i < n; ++i) { cx2 += pos[i].x; cy2 += pos[i].y; }
    cx2 /= n; cy2 /= n;
    double shiftx = w / 2.0 - cx2, shifty = h / 2.0 - cy2;
    for (size_t i = 0; i < n; ++i) { pos[i].x += shiftx; pos[i].y += shifty; }

    // ── Emit JSON data for client-side rendering ──
    auto d2s = [](double v) {
        char buf[16]; std::snprintf(buf, sizeof(buf), "%.1f", v);
        return std::string(buf);
    };

    std::string json = "{\"w\":" + std::to_string(w) + ",\"h\":" + std::to_string(h) + ",\"nodes\":[";
    for (size_t i = 0; i < n; ++i)
    {
        if (i > 0) json += ',';
        double r = 5.0 + std::min(degree[i], 12) * 0.5;
        std::string title = posts[i].title.get();
        // Escape for JSON
        std::string esc_title;
        for (char c : title)
        {
            if (c == '"') esc_title += "\\\"";
            else if (c == '\\') esc_title += "\\\\";
            else esc_title += c;
        }
        json += "{\"x\":" + d2s(pos[i].x) + ",\"y\":" + d2s(pos[i].y)
              + ",\"r\":" + d2s(r) + ",\"d\":" + std::to_string(degree[i])
              + ",\"s\":\"" + posts[i].slug.get() + "\",\"t\":\"" + esc_title + "\"}";
    }
    json += "],\"edges\":[";
    for (size_t i = 0; i < edges.size(); ++i)
    {
        if (i > 0) json += ',';
        json += "{\"a\":" + std::to_string(edges[i].a) + ",\"b\":" + std::to_string(edges[i].b)
              + ",\"w\":" + std::to_string(edges[i].weight) + "}";
    }
    json += "]}";

    return section(class_("post-graph-section"),
        h3("Post Connections"),
        div(id("post-graph-container")),
        script(attr("type", "application/json"), id("post-graph-data"), raw(json)),
        script(raw(POSTGRAPH_JS))
    );
}

// ═══════════════════════════════════════════════════════════════════════
//  Ctx helpers
// ═══════════════════════════════════════════════════════════════════════

Node Ctx::page(const PageMeta& meta, Node content,
               const SidebarData* sidebar_data) const
{
    bool has_sb = sidebar_data && !site.sidebar.widgets.empty();

    auto breadcrumbs_node = operator()(Breadcrumbs{
        .canonical_path = meta.canonical_path,
        .og_type        = meta.og_type,
        .page_title     = meta.title
    });

    auto content_area = has_sb
        ? operator()(ContentArea{
            .has_sidebar = true,
            .sidebar_node = operator()(Sidebar{.data = sidebar_data})
          },
            breadcrumbs_node,
            std::move(content))
        : operator()(ContentArea{.has_sidebar = false},
            breadcrumbs_node,
            std::move(content));

    auto doc = operator()(Document{.page_meta = &meta},
        operator()(Header{}),
        content_area,
        operator()(Footer{})
    );

    // Post-process: add sidebar body class and external link targets
    auto result = doc.render();

    // Inject has-sidebar class into <body>
    if (has_sb)
    {
        auto body_pos = result.find("<body");
        if (body_pos != std::string::npos)
        {
            auto class_pos = result.find("class=\"", body_pos);
            if (class_pos != std::string::npos && class_pos < result.find('>', body_pos))
            {
                auto quote_pos = result.find('"', class_pos + 7);
                if (quote_pos != std::string::npos)
                    result.insert(quote_pos, " has-sidebar");
            }
        }
    }

    if (site.layout.external_links_new_tab)
        result = apply_external_new_tab(result);

    // Inject back-to-top button and UX scripts before </body>
    {
        std::string inject;
        inject += "<a class=\"back-to-top\" id=\"backToTop\" href=\"#\" title=\"Back to top\">&uarr;</a>";
        if (assets && !assets->js_url.empty())
        {
            // External JS: content-hashed, immutably cached, deferred
            inject += "<script src=\"";
            inject += assets->js_url;
            inject += "\" defer></script>";
        }
        else
        {
            // Fallback: inline JS
            inject += "<script>(function(){var b=document.getElementById('backToTop');"
                "if(b)window.addEventListener('scroll',function(){"
                "b.classList.toggle('visible',window.scrollY>400)},{passive:true});})()</script>";
            inject += "<script>";
            inject += CMD_PALETTE_JS;
            inject += KEYBOARD_NAV_JS;
            inject += ACTIVE_TOC_JS;
            inject += IMAGE_ZOOM_JS;
            inject += READING_POS_JS;
            inject += CODE_COPY_JS;
            inject += "</script>";
        }
        auto body_end = result.rfind("</body>");
        if (body_end != std::string::npos)
            result.insert(body_end, inject);
    }

    // Return as raw node so it can be composed further if needed
    return dom::raw(std::move(result));
}

std::string build_combined_css(const Site& site)
{
    std::string css;

    // Base CSS (or custom override)
    css += site.theme.css.empty() ? DEFAULT_CSS : site.theme.css;

    // Theme CSS
    if (site.theme.name != "default" && site.theme.name != "custom")
    {
        auto& themes = builtin_themes();
        auto it = themes.find(site.theme.name);
        if (it != themes.end()) css += theme::compile(it->second);
    }

    // Variable overrides
    if (!site.theme.variables.empty())
    {
        std::string lv, dv;
        for (const auto& [key, value] : site.theme.variables)
        {
            if (key.substr(0, 5) == "dark-") dv += "--" + key.substr(5) + ":" + value + ";";
            else lv += "--" + key + ":" + value + ";";
        }
        if (!lv.empty()) css += ":root{" + lv + "}";
        if (!dv.empty()) css += "[data-theme=\"dark\"]{" + dv + "}";
    }

    // Custom CSS
    if (!site.layout.custom_css.empty())
        css += site.layout.custom_css;

    return css;
}

std::string build_ux_js_bundle()
{
    std::string js;

    // Back-to-top scroll handler
    js += "(function(){var b=document.getElementById('backToTop');"
          "if(b)window.addEventListener('scroll',function(){"
          "b.classList.toggle('visible',window.scrollY>400)},{passive:true});})();";

    js += CMD_PALETTE_JS;
    js += KEYBOARD_NAV_JS;
    js += ACTIVE_TOC_JS;
    js += IMAGE_ZOOM_JS;
    js += READING_POS_JS;
    js += CODE_COPY_JS;

    return js;
}

Ctx Ctx::from(const Site& site, const AssetManifest* assets)
{
    // Resolve theme component overrides and ThemeDef pointer
    auto& themes = builtin_themes();
    auto it = themes.find(site.theme.name);
    if (it != themes.end())
        return {site,
                it->second.components ? it->second.components.get() : nullptr,
                &it->second,
                assets};

    return {site, nullptr, nullptr, assets};
}

} // namespace loom::component
