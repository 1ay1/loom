#include "../../include/loom/render/component.hpp"
#include "../../include/loom/render/themes.hpp"
#include "../../include/loom/render/theme/theme_def.hpp"
#include "../../include/loom/render/base_styles.hpp"

#include <ctime>

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
        when(layout.show_theme_toggle, script(dom::raw(THEME_JS))),
        when(!layout.custom_head_html.empty(), dom::raw(layout.custom_head_html)),
        style(dom::raw(site.theme.css.empty() ? DEFAULT_CSS : site.theme.css)),
        when(!theme_css.empty(), style(dom::raw(theme_css))),
        when(!var_css.empty(), style(dom::raw(var_css))),
        when(!layout.custom_css.empty(), style(dom::raw(layout.custom_css)))
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

    return article(class_("post-full"),
        ctx(ReadingProgress{}),
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

    return section(
        h2("Archives"),
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

    // Inject back-to-top button before </body>
    {
        static const char BTT[] =
            "<a class=\"back-to-top\" id=\"backToTop\" href=\"#\" title=\"Back to top\">&uarr;</a>"
            "<script>(function(){var b=document.getElementById('backToTop');"
            "if(b)window.addEventListener('scroll',function(){"
            "b.classList.toggle('visible',window.scrollY>400)},{passive:true});})()</script>";
        auto body_end = result.rfind("</body>");
        if (body_end != std::string::npos)
            result.insert(body_end, BTT);
    }

    // Return as raw node so it can be composed further if needed
    return dom::raw(std::move(result));
}

Ctx Ctx::from(const Site& site)
{
    // Resolve theme component overrides and ThemeDef pointer
    auto& themes = builtin_themes();
    auto it = themes.find(site.theme.name);
    if (it != themes.end())
        return {site,
                it->second.components ? it->second.components.get() : nullptr,
                &it->second};

    return {site, nullptr, nullptr};
}

} // namespace loom::component
