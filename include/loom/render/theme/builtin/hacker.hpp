#pragma once
#include "../theme_def.hpp"
#include "../../component.hpp"
#include <ctime>

namespace loom::theme::builtin
{
using namespace css;
using namespace ui;

// ── Hacker — clean CRT phosphor terminal ──
//
// Nerdy and geeky but with breathing room. Scanlines, phosphor glow,
// blinking cursor, clean columnar listings, tree-style series nav.

namespace {
    const auto phosphor = hex("#00ff41");
    const auto dim_p    = hex("#00aa2a");
    const auto black    = hex("#0a0a0a");
    const auto dark_g   = hex("#0d1a0d");
    const auto gray     = hex("#2a2a2a");
    const auto dim_txt  = hex("#5a9a5a");
    const auto glow     = raw("0 0 8px rgba(0,255,65,0.3)");

    std::string fmt_date_short(std::chrono::system_clock::time_point tp) {
        auto t = std::chrono::system_clock::to_time_t(tp);
        std::tm tm{}; gmtime_r(&t, &tm);
        char buf[16]; std::strftime(buf, sizeof(buf), "%b %d", &tm);
        return buf;
    }

    std::string fmt_size(int minutes) {
        int kb = minutes * 1200;
        if (kb >= 1000) return std::to_string(kb / 1000) + "." + std::to_string((kb % 1000) / 100) + "K";
        return std::to_string(kb);
    }
}

inline const ThemeDef hacker = {
    .light = {{black.v}, {phosphor.v}, {dim_txt.v}, {gray.v}, {phosphor.v}},
    .dark  = {{black.v}, {phosphor.v}, {dim_txt.v}, {gray.v}, {phosphor.v}},
    .font  = {"'Courier New',Courier,ui-monospace,'SF Mono',monospace"},
    .font_size = "13px",
    .max_width = "740px",
    .line_height = "1.6",
    .heading_weight = "400",
    .header_size = "13px",
    .corners = Corners::Sharp,
    .density = Density::Compact,
    .border_weight = BorderWeight::Thin,
    .nav_style = NavStyle::Minimal,
    .tag_style = TagStyle::Plain,
    .link_style = LinkStyle::None,
    .code_style = CodeBlockStyle::Plain,
    .inline_code = InlineCodeStyle::Plain,
    .card_hover = CardHover::None,
    .hr_style = HrStyle::Dashed,
    .table_style = TableStyle::Minimal,
    .sidebar_style = SidebarStyle::Clean,
    .scrollbar = Scrollbar::Hidden,
    .focus_style = FocusStyle::None,
    .date_format = "%Y-%m-%d",
    .index_heading = "~",
    .styles = sheet(
        ".theme-toggle"_s | display(none),

        // ── Scanline overlay ──
        "body::after"_s
            | prop("content", raw("''")) | position(fixed) | inset(0_px)
            | prop("pointer-events", none) | z_index(9999) | opacity(0.02)
            | background_image(raw(
                "repeating-linear-gradient(0deg,transparent,transparent 1px,"
                "rgba(0,255,65,0.03) 1px,rgba(0,255,65,0.03) 2px)")),

        // ── CRT vignette ──
        "body"_s | bg(black)
                 | background_image(raw("radial-gradient(ellipse at 50% 50%,#0d1a0d 0%,#0a0a0a 80%)"))
                 | color(phosphor),

        "*"_s | border_color(gray),
        "a"_s | color(phosphor),
        "a:hover"_s | color(raw("#50ff80")) | text_decoration(none),

        // ── Header — same size as everything, hierarchy via color ──
        "header"_s | border_bottom(none) | padding_bottom(12_px)
                   | margin_bottom(8_px),
        "header h1"_s | font_size(14_px) | font_weight(400) | letter_spacing(0_px)
                      | margin_bottom(0_px) | color(phosphor)
                      | prop("text-shadow", glow),
        "header h1 a"_s | color(phosphor) | text_decoration(none),
        "header h1 a::before"_s | prop("content", raw("'~$ '")),
        "header h1 a:hover"_s | color(raw("#50ff80")),
        ".site-description"_s | color(dim_txt) | font_size(13_px) | margin_top(4_px),
        ".site-description::before"_s | prop("content", raw("'# '")),
        "nav"_s | margin_top(8_px),
        "nav a"_s | color(dim_p) | font_size(13_px),
        "nav a::before"_s | prop("content", raw("'./'")),
        "nav a:hover"_s | color(phosphor),

        // ── Post listings ──
        ".post-listing"_s | padding(8_px, 0_px) | border_bottom(1_px, dashed, gray)
                          | line_height(num(1.5)),
        ".post-listing:hover"_s | bg(dark_g),
        ".post-listing > a"_s | color(phosphor) | font_weight(400)
                              | font_size(14_px) | display(raw("inline")),
        ".post-listing > a:hover"_s | color(raw("#50ff80")),
        ".post-listing-meta"_s | display(none),
        ".post-listing .excerpt"_s | display(none),
        ".post-listing .post-tags"_s | display(none),
        // Custom spans from component override
        ".ls-inline"_s | color(dim_txt) | font_size(12_px) | margin_left(10_px),
        ".ls-excerpt"_s | color(dim_txt) | font_size(12_px) | margin_top(3_px)
                        | line_height(num(1.5)),
        ".ls-tags"_s | color(dim_txt) | font_size(12_px) | margin_top(3_px),

        // ── Section headings ──
        "h2"_s | border_bottom(none) | font_size(13_px) | font_weight(400)
               | color(dim_p) | letter_spacing(1_px) | text_transform(uppercase)
               | margin_top(24_px) | margin_bottom(8_px) | padding_bottom(0_px),

        // ── Tags ──
        vars({{"tag-bg", transparent}, {"tag-text", dim_txt}, {"tag-radius", raw("0")},
              {"tag-hover-bg", transparent}, {"tag-hover-text", phosphor}}),
        ".tag"_s | font_size(12_px) | padding(0_px, 2_px),
        ".tag::before"_s | prop("content", raw("'--'")),
        ".tag:hover"_s | color(phosphor),

        // ── Cards ──
        ".post-card"_s | bg(transparent) | border(1_px, dashed, gray) | padding(14_px),
        ".post-card a"_s | color(phosphor) | font_weight(400) | font_size(14_px),
        ".post-card a:hover"_s | color(raw("#50ff80")),
        ".post-card:hover"_s | border_color(dim_p) | bg(dark_g),
        ".post-card .date"_s | color(dim_txt) | font_size(12_px),
        ".post-card .reading-time"_s | display(none),
        ".post-card .excerpt"_s | color(dim_txt) | font_size(12_px),

        // ── Post page ──
        ".post-full h1"_s | font_size(15_px) | font_weight(400) | color(phosphor)
                          | margin_bottom(6_px) | line_height(num(1.4))
                          | text_transform(none)
                          | prop("text-shadow", glow),
        ".post-full h1::before"_s | prop("content", raw("'> '")),
        ".post-meta"_s | color(dim_txt) | font_size(12_px),
        ".heading-anchor"_s | opacity(0.2),

        // ── Content area — body text slightly smaller than UI ──
        ".post-content,.page-content"_s | font_size(raw("12.5px")),
        content_area().nest(
            "a"_s | color(phosphor) | text_decoration(underline)
                  | text_decoration_style(dotted) | text_underline_offset(3_px),
            "a:hover"_s | text_decoration_style(solid),
            "pre"_s | border_left(2_px, solid, dim_p) | font_size(12_px),
            ":not(pre)>code"_s | color(phosphor) | bg(transparent) | padding(0_px),
            "h1,h2,h3,h4"_s | color(phosphor) | font_weight(400),
            "h2"_s | font_size(13_px) | margin_top(28_px) | border_bottom(none),
            "h3"_s | font_size(12_px),
            "hr"_s | border_top(1_px, dashed, gray),
            "blockquote"_s | border_left(2_px, solid, dim_p)
                           | color(dim_txt) | bg(transparent) | padding(4_px, 14_px),
            "table th"_s | bg(transparent) | color(phosphor),
            "table th,table td"_s | border_color(gray)
        ),

        // ── Post nav ──
        ".post-nav"_s | border_top(1_px, dashed, gray) | margin_top(28_px)
                      | padding_top(12_px),
        ".post-nav a"_s | color(dim_p) | font_size(13_px),
        ".post-nav a:hover"_s | color(phosphor),
        ".related-posts"_s | border_top(1_px, dashed, gray),
        ".related-posts h2"_s | color(dim_txt),
        ".related-posts li a"_s | color(phosphor) | font_size(13_px),
        ".related-posts li a:hover"_s | color(raw("#50ff80")),

        // ── Series nav — tree style ──
        ".series-nav"_s | border_color(gray) | border_left_color(dim_p) | bg(transparent),
        ".series-label"_s | color(dim_txt) | font_size(13_px),
        ".series-list"_s | list_style(none) | padding_left(0_px),
        ".series-list li"_s | padding_left(16_px),
        ".series-list li::before"_s | prop("content", raw("'\\251C\\2500 '"))
                                    | color(gray),
        ".series-list li:last-child::before"_s | prop("content", raw("'\\2514\\2500 '"))
                                               | color(gray),
        ".series-list a"_s | color(phosphor) | font_size(13_px),
        ".series-current"_s | color(phosphor) | font_weight(400),
        ".series-current::after"_s | prop("content", raw("' *'"))
                                   | color(dim_p),

        // ── Series/archive page — dates on their own line so they don't wrap ──
        ".series-list .date"_s | display(block) | font_size(12_px) | color(dim_txt)
                               | margin_left(0_px),

        // ── Sidebar ──
        ".sidebar"_s | font_size(13_px),
        ".widget h3"_s | font_size(13_px) | color(dim_txt) | font_weight(400)
                       | letter_spacing(1_px) | text_transform(uppercase)
                       | border_bottom(1_px, dashed, gray) | padding_bottom(4_px),
        ".widget li a"_s | color(phosphor) | font_size(13_px),
        ".widget li a:hover"_s | color(raw("#50ff80")),
        ".widget .date"_s | color(dim_txt) | font_size(12_px) | display(block),
        ".widget p"_s | color(dim_txt) | font_size(13_px),

        // ── Footer ──
        "footer"_s | border_top(1_px, dashed, gray) | color(dim_txt) | font_size(12_px),
        ".footer-links a"_s | color(dim_p),
        ".footer-links a:hover"_s | color(phosphor),

        // ── Breadcrumbs ──
        "nav.breadcrumb"_s | color(dim_txt) | font_size(13_px),
        "nav.breadcrumb a"_s | color(dim_txt),
        "nav.breadcrumb a:hover"_s | color(phosphor),

        // ── Blinking cursor ──
        keyframes("blink",
            from() | opacity(1.0),
            to()   | opacity(0.0)
        ),
        "header h1 a::after"_s
            | prop("content", raw("'\\2588'")) | color(phosphor)
            | margin_left(2_px)
            | prop("animation", raw("blink 1s step-end infinite")),

        // ── Selection ──
        "::selection"_s | bg(phosphor) | color(black)
    ),

    .components = overrides({
        .header = [](const Header&, const Ctx& ctx, Children) {
            const auto& s = ctx.site;
            return dom::header(
                div(class_("container header-bar"),
                    div(class_("header-left"),
                        h1(a(href("/"), s.title)),
                        when(s.layout.show_description && !s.description.empty(),
                            p_(class_("site-description"), s.description)),
                        ctx(Nav{}))));
        },

        .footer = [](const component::Footer&, const Ctx& ctx, Children) {
            auto pid = std::to_string(::getpid());
            return dom::footer(
                div(class_("container"),
                    p_(dom::raw(
                        "<span style='color:#3a6a3a'>PID " + pid + "</span>"
                        " &middot; "
                        + (!ctx.site.footer.copyright.empty()
                            ? ctx.site.footer.copyright
                            : std::string("process exited with code 0")))),
                    when(!ctx.site.footer.links.empty(),
                        div(class_("footer-links"),
                            each(ctx.site.footer.links, [](const auto& l) {
                                return a(href(l.url), l.title); })))));
        },

        .post_listing = [](const PostListing& props, const Ctx&, Children) {
            if (!props.post) return empty();
            const auto& p = *props.post;

            std::string tags_str;
            for (const auto& t : p.tags) {
                if (!tags_str.empty()) tags_str += "  ";
                tags_str += "--" + t.get();
            }

            return article(class_("post-listing"),
                // Line 1: title  date  size — all inline
                a(href("/post/" + p.slug.get()), p.title.get()),
                span(class_("ls-inline"),
                    fmt_date_short(p.published) + "  "
                    + fmt_size(p.reading_time_minutes)),
                // Line 2: excerpt
                when(!p.excerpt.empty(),
                    p_(class_("ls-excerpt"), p.excerpt)),
                // Line 3: tags
                when(!tags_str.empty(),
                    div(class_("ls-tags"), tags_str)));
        },

        .index = [](const Index& props, const Ctx& ctx, Children) {
            if (!props.posts) return empty();
            return section(
                h2(dom::raw("~/ &nbsp;<span style='color:#3a6a3a;font-weight:400;"
                    "text-transform:none;letter-spacing:0'>" +
                    std::to_string(props.posts->size()) + " posts</span>")),
                each(*props.posts, [&](const PostSummary& p) {
                    return ctx(PostListing{.post = &p});
                }));
        },
    }),
};

} // namespace loom::theme::builtin
