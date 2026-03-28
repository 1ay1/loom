#pragma once
#include "../theme_def.hpp"
#include "../../component.hpp"
#include <ctime>

namespace loom::theme::builtin
{
using namespace css;
using namespace ui;

// ── Hacker — CRT phosphor terminal, radically different UI ──
//
// Shows what the theming system can do: custom component HTML,
// scanline overlay, blinking cursor, prompt-style headings,
// ls-style post listings, everything monospace green-on-black.

namespace {
    const auto phosphor = hex("#00ff41");
    const auto dim_p    = hex("#00aa2a");
    const auto black    = hex("#0a0a0a");
    const auto dark_g   = hex("#0d1a0d");
    const auto faint_g  = hex("#143a14");
    const auto mid_g    = hex("#1a4a1a");
    const auto gray     = hex("#2a2a2a");
    const auto dim_txt  = hex("#4a7a4a");
}

inline const ThemeDef hacker = {
    // Both palettes identical — no light mode, this is a terminal
    .light = {{black.v}, {phosphor.v}, {dim_p.v}, {gray.v}, {phosphor.v}},
    .dark  = {{black.v}, {phosphor.v}, {dim_p.v}, {gray.v}, {phosphor.v}},
    .font  = {"'Courier New',Courier,ui-monospace,'SF Mono',monospace"},
    .font_size = "13px",
    .max_width = "760px",
    .line_height = "1.5",
    .heading_weight = "400",
    .header_size = "16px",
    .corners = Corners::Sharp,
    .density = Density::Compact,
    .border_weight = BorderWeight::Thin,
    .nav_style = NavStyle::Minimal,
    .tag_style = TagStyle::Plain,
    .link_style = LinkStyle::None,
    .code_style = CodeBlockStyle::Plain,
    .inline_code = InlineCodeStyle::Plain,
    .heading_case = HeadingCase::Upper,
    .card_hover = CardHover::None,
    .hr_style = HrStyle::Dashed,
    .table_style = TableStyle::Minimal,
    .sidebar_style = SidebarStyle::Clean,
    .scrollbar = Scrollbar::Hidden,
    .focus_style = FocusStyle::None,
    .date_format = "%Y-%m-%d",
    .index_heading = "> ls -lt ./posts/",
    .styles = sheet(
        // ── Kill dark toggle ──
        ".theme-toggle"_s | display(none),

        // ── Scanline overlay on body ──
        "body::after"_s
            | prop("content", raw("''")) | position(fixed) | inset(0_px)
            | prop("pointer-events", none)
            | z_index(9999) | opacity(0.03)
            | background_image(raw(
                "repeating-linear-gradient(0deg,transparent,transparent 1px,rgba(0,255,65,0.03) 1px,rgba(0,255,65,0.03) 2px)")),

        // ── Subtle CRT glow ──
        "body"_s | bg(black)
                 | background_image(raw("radial-gradient(ellipse at 50% 50%,#0d1a0d 0%,#0a0a0a 80%)"))
                 | color(phosphor),

        // ── Global text ──
        "*"_s | border_color(gray),
        "a"_s | color(phosphor),
        "a:hover"_s | color(raw("#50ff80")) | text_decoration(none),

        // ── Header as terminal prompt ──
        "header"_s | border_bottom(none) | padding_bottom(8_px),
        "header h1"_s | font_size(16_px) | font_weight(400) | letter_spacing(0_px)
                      | margin_bottom(0_px) | color(phosphor),
        "header h1 a"_s | color(phosphor) | text_decoration(none),
        "header h1 a::before"_s | prop("content", raw("'root@blog:~$ '")),
        "header h1 a:hover"_s | color(raw("#50ff80")),
        ".site-description"_s | color(dim_p) | font_size(12_px)
                              | margin_top(2_px),
        ".site-description::before"_s | prop("content", raw("'# '")),
        "nav"_s | margin_top(6_px),
        "nav a"_s | color(dim_p) | font_size(12_px),
        "nav a::before"_s | prop("content", raw("'./'")),
        "nav a:hover"_s | color(phosphor),

        // ── Post listings as file listing ──
        ".post-listing"_s | padding(3_px, 0_px) | border_bottom(none)
                          | font_size(13_px) | line_height(num(1.4)),
        ".post-listing:hover"_s | bg(dark_g),
        ".post-listing > a"_s | color(phosphor) | font_weight(400)
                              | font_size(13_px) | display(block),
        ".post-listing > a:hover"_s | color(raw("#50ff80")),
        ".post-listing-meta"_s | display(flex) | gap(8_px)
                               | margin_top(1_px) | font_size(11_px) | color(dim_p),
        ".post-listing .reading-time::before"_s | prop("content", none),
        ".post-listing .excerpt"_s | display(none),
        ".post-listing .post-tags"_s | display(raw("inline")) | margin_top(0_px) | margin_left(0_px),

        // ── Section headings as comments ──
        "h2"_s | border_bottom(none) | font_size(12_px) | font_weight(400)
               | color(dim_p) | letter_spacing(1_px) | text_transform(none)
               | margin_top(16_px) | margin_bottom(8_px) | padding_bottom(0_px),
        "h2::before"_s | prop("content", raw("'// '")),

        // ── Tags as flags ──
        vars({{"tag-bg", transparent}, {"tag-text", dim_p}, {"tag-radius", raw("0")},
              {"tag-hover-bg", transparent}, {"tag-hover-text", phosphor}}),
        ".tag"_s | font_size(11_px) | padding(0_px, 2_px),
        ".tag::before"_s | prop("content", raw("'--'")),
        ".tag:hover"_s | color(phosphor),

        // ── Cards ──
        ".post-card"_s | bg(transparent) | border(1_px, dashed, gray)
                       | padding(12_px),
        ".post-card a"_s | color(phosphor) | font_weight(400) | font_size(13_px),
        ".post-card a:hover"_s | color(raw("#50ff80")),
        ".post-card:hover"_s | border_color(dim_p) | bg(dark_g),
        ".post-card .date"_s | color(dim_p) | font_size(12_px),
        ".post-card .reading-time"_s | display(none),
        ".post-card .excerpt"_s | color(dim_txt) | font_size(12_px),

        // ── Post page ──
        ".post-full h1"_s | font_size(16_px) | font_weight(400) | color(phosphor)
                          | margin_bottom(4_px) | line_height(num(1.4))
                          | text_transform(none),
        ".post-full h1::before"_s | prop("content", raw("'cat '")),
        ".post-meta"_s | color(dim_p) | font_size(12_px),
        ".post-meta::before"_s | prop("content", raw("'# '")),
        ".heading-anchor"_s | opacity(0.3),

        // ── Content area ──
        content_area().nest(
            "a"_s | color(phosphor) | text_decoration(underline)
                  | text_decoration_style(dotted) | text_underline_offset(3_px),
            "a:hover"_s | text_decoration_style(solid),
            "pre"_s | border(1_px, dashed, gray)
                    | border_left(3_px, solid, dim_p),
            ":not(pre)>code"_s | color(phosphor) | bg(transparent)
                               | padding(0_px),
            "h1,h2,h3,h4"_s | color(phosphor) | font_weight(400),
            "h2"_s | font_size(14_px) | margin_top(24_px) | border_bottom(none),
            "h2::before"_s | prop("content", raw("'## '")),
            "h3"_s | font_size(13_px),
            "h3::before"_s | prop("content", raw("'### '")),
            "hr"_s | border_top(1_px, dashed, gray),
            "blockquote"_s | border_left(3_px, solid, dim_p)
                           | color(dim_p) | bg(transparent)
                           | padding(4_px, 12_px),
            "table th"_s | bg(transparent) | color(phosphor),
            "table th,table td"_s | border_color(gray)
        ),

        // ── Post nav ──
        ".post-nav"_s | border_top(1_px, dashed, gray) | margin_top(24_px),
        ".post-nav a"_s | color(dim_p) | font_size(12_px),
        ".post-nav a:hover"_s | color(phosphor),
        ".related-posts"_s | border_top(1_px, dashed, gray),
        ".related-posts h2"_s | color(dim_p),
        ".related-posts li a"_s | color(phosphor) | font_size(13_px),
        ".related-posts li a:hover"_s | color(raw("#50ff80")),

        // ── Sidebar ──
        ".sidebar"_s | font_size(12_px),
        ".widget h3"_s | font_size(11_px) | color(dim_p) | font_weight(400)
                       | letter_spacing(1_px) | border_bottom(1_px, dashed, gray)
                       | padding_bottom(4_px),
        ".widget h3::before"_s | prop("content", raw("'# '")),
        ".widget li a"_s | color(phosphor) | font_size(12_px),
        ".widget li a:hover"_s | color(raw("#50ff80")),
        ".widget .date"_s | color(dim_p),
        ".widget p"_s | color(dim_p),

        // ── Footer ──
        "footer"_s | border_top(1_px, dashed, gray) | color(dim_p) | font_size(12_px),
        "footer::before"_s | prop("content", raw("'# '")),
        ".footer-links a"_s | color(dim_p),
        ".footer-links a:hover"_s | color(phosphor),

        // ── Chrome ──
        "nav.breadcrumb"_s | color(dim_p) | font_size(12_px),
        "nav.breadcrumb a"_s | color(dim_p),
        "nav.breadcrumb a:hover"_s | color(phosphor),
        ".series-nav"_s | border_color(gray) | border_left_color(dim_p) | bg(transparent),
        ".series-label"_s | color(dim_p),
        ".series-list a"_s | color(phosphor),
        ".series-current"_s | color(phosphor),

        // ── Blinking cursor after site title ──
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

    // ── Component overrides — show the full power ──
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
            return dom::footer(
                div(class_("container"),
                    p_(dom::raw(
                        !ctx.site.footer.copyright.empty()
                            ? ctx.site.footer.copyright
                            : std::string("EOF"))),
                    when(!ctx.site.footer.links.empty(),
                        div(class_("footer-links"),
                            each(ctx.site.footer.links, [](const auto& l) {
                                return a(href(l.url), l.title); })))));
        },

        .index = [](const Index& props, const Ctx& ctx, Children) {
            if (!props.posts) return empty();
            return section(
                h2("> ls -lt ./posts/"),
                p_(class_("ls-header"),
                    dom::raw("<span style='color:#00aa2a;font-size:11px'>"
                             "total " + std::to_string(props.posts->size()) +
                             "</span>")),
                each(*props.posts, [&](const PostSummary& p) {
                    return ctx(PostListing{.post = &p});
                }));
        },
    }),
};

} // namespace loom::theme::builtin
