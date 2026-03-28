#pragma once
#include "../theme_def.hpp"
#include "../../component.hpp"
#include <ctime>

namespace loom::theme::builtin
{
using namespace css;
using namespace ui;

// ── Hacker — CRT phosphor terminal, maximum nerd energy ──
//
// Scanlines, phosphor glow, blinking cursor, ls -l style listings
// with file permissions, tree-style series nav, process-style footer,
// custom components showing the full power of the theming system.

namespace {
    const auto phosphor = hex("#00ff41");
    const auto dim_p    = hex("#00aa2a");
    const auto black    = hex("#0a0a0a");
    const auto dark_g   = hex("#0d1a0d");
    const auto gray     = hex("#2a2a2a");
    const auto dim_txt  = hex("#4a7a4a");
    const auto glow     = raw("0 0 8px rgba(0,255,65,0.4)");

    std::string fmt_date_short(std::chrono::system_clock::time_point tp) {
        auto t = std::chrono::system_clock::to_time_t(tp);
        std::tm tm{}; gmtime_r(&t, &tm);
        char buf[16]; std::strftime(buf, sizeof(buf), "%b %d", &tm);
        return buf;
    }

    std::string fmt_size(int minutes) {
        int kb = minutes * 1200; // ~1.2K per minute of reading
        if (kb >= 1000) {
            auto s = std::to_string(kb / 1000);
            s += '.';
            s += std::to_string((kb % 1000) / 100);
            s += 'K';
            return s;
        }
        return std::to_string(kb);
    }
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

        // ── Scanline overlay ──
        "body::after"_s
            | prop("content", raw("''")) | position(fixed) | inset(0_px)
            | prop("pointer-events", none) | z_index(9999) | opacity(0.03)
            | background_image(raw(
                "repeating-linear-gradient(0deg,transparent,transparent 1px,"
                "rgba(0,255,65,0.03) 1px,rgba(0,255,65,0.03) 2px)")),

        // ── CRT glow + vignette ──
        "body"_s | bg(black)
                 | background_image(raw("radial-gradient(ellipse at 50% 50%,#0d1a0d 0%,#0a0a0a 80%)"))
                 | color(phosphor),

        // ── Global phosphor glow on text ──
        "*"_s | border_color(gray),
        "a"_s | color(phosphor),
        "a:hover"_s | color(raw("#50ff80")) | text_decoration(none)
                    | prop("text-shadow", raw("0 0 6px rgba(0,255,65,0.5)")),

        // ── Header — boot sequence style ──
        "header"_s | border_bottom(none) | padding_bottom(8_px),
        "header h1"_s | font_size(16_px) | font_weight(400) | letter_spacing(0_px)
                      | margin_bottom(0_px) | color(phosphor)
                      | prop("text-shadow", glow),
        "header h1 a"_s | color(phosphor) | text_decoration(none),
        "header h1 a::before"_s | prop("content", raw("'root@blog:~$ '")),
        "header h1 a:hover"_s | color(raw("#50ff80")),
        ".site-description"_s | color(dim_p) | font_size(12_px) | margin_top(2_px),
        ".site-description::before"_s | prop("content", raw("'# '")),
        "nav"_s | margin_top(6_px),
        "nav a"_s | color(dim_p) | font_size(12_px),
        "nav a::before"_s | prop("content", raw("'./'")),
        "nav a:hover"_s | color(phosphor),

        // ── Post listings — ls -l style (via component override) ──
        ".post-listing"_s | padding(1_px, 0_px) | border_bottom(none)
                          | font_size(13_px) | line_height(num(1.6)),
        ".post-listing:hover"_s | bg(dark_g),
        ".post-listing > a"_s | color(phosphor) | font_weight(400)
                              | font_size(13_px) | display(raw("inline")),
        ".post-listing > a:hover"_s | color(raw("#50ff80"))
                                    | prop("text-shadow", raw("0 0 6px rgba(0,255,65,0.5)")),
        ".post-listing-meta"_s | display(none),
        ".post-listing .excerpt"_s | display(none),
        ".post-listing .post-tags"_s | display(none),
        // ls-l metadata styled via component override spans
        ".ls-perms"_s | color(dim_txt) | font_size(12_px),
        ".ls-user"_s | color(dim_p) | font_size(12_px),
        ".ls-size"_s | color(dim_p) | font_size(12_px) | display(raw("inline-block"))
                     | width(40_px) | text_align(right) | margin_right(8_px),
        ".ls-date"_s | color(dim_p) | font_size(12_px) | margin_right(8_px),
        ".ls-tags"_s | color(dim_txt) | font_size(11_px) | margin_left(16_px),

        // ── Section headings as comments ──
        "h2"_s | border_bottom(none) | font_size(12_px) | font_weight(400)
               | color(dim_p) | letter_spacing(1_px) | text_transform(none)
               | margin_top(16_px) | margin_bottom(4_px) | padding_bottom(0_px),
        "h2::before"_s | prop("content", raw("'// '")),

        // ── Tags as flags ──
        vars({{"tag-bg", transparent}, {"tag-text", dim_p}, {"tag-radius", raw("0")},
              {"tag-hover-bg", transparent}, {"tag-hover-text", phosphor}}),
        ".tag"_s | font_size(11_px) | padding(0_px, 2_px),
        ".tag::before"_s | prop("content", raw("'--'")),
        ".tag:hover"_s | color(phosphor),

        // ── Cards — dashed box ──
        ".post-card"_s | bg(transparent) | border(1_px, dashed, gray) | padding(12_px),
        ".post-card a"_s | color(phosphor) | font_weight(400) | font_size(13_px),
        ".post-card a:hover"_s | color(raw("#50ff80")),
        ".post-card:hover"_s | border_color(dim_p) | bg(dark_g),
        ".post-card .date"_s | color(dim_p) | font_size(12_px),
        ".post-card .reading-time"_s | display(none),
        ".post-card .excerpt"_s | color(dim_txt) | font_size(12_px),

        // ── Post page — cat file style ──
        ".post-full h1"_s | font_size(15_px) | font_weight(400) | color(phosphor)
                          | margin_bottom(4_px) | line_height(num(1.4))
                          | text_transform(none)
                          | prop("text-shadow", glow),
        ".post-full h1::before"_s | prop("content", raw("'$ cat '")),
        ".post-meta"_s | color(dim_p) | font_size(12_px),
        ".post-meta .reading-time::before"_s | prop("content", raw("' | wc -w ~'")),
        ".heading-anchor"_s | opacity(0.3),

        // ── Content area ──
        content_area().nest(
            "a"_s | color(phosphor) | text_decoration(underline)
                  | text_decoration_style(dotted) | text_underline_offset(3_px),
            "a:hover"_s | text_decoration_style(solid),
            "pre"_s | border(1_px, dashed, gray) | border_left(3_px, solid, dim_p),
            ":not(pre)>code"_s | color(phosphor) | bg(transparent) | padding(0_px),
            "h1,h2,h3,h4"_s | color(phosphor) | font_weight(400),
            "h2"_s | font_size(14_px) | margin_top(24_px) | border_bottom(none),
            "h2::before"_s | prop("content", raw("'## '")),
            "h3"_s | font_size(13_px),
            "h3::before"_s | prop("content", raw("'### '")),
            "hr"_s | border_top(1_px, dashed, gray),
            "blockquote"_s | border_left(3_px, solid, dim_p)
                           | color(dim_p) | bg(transparent) | padding(4_px, 12_px),
            "blockquote::before"_s | prop("content", raw("'> '")),
            "table th"_s | bg(transparent) | color(phosphor),
            "table th,table td"_s | border_color(gray)
        ),

        // ── Post nav — cd prev/next ──
        ".post-nav"_s | border_top(1_px, dashed, gray) | margin_top(24_px),
        ".post-nav-prev::before"_s | prop("content", raw("'cd ../ '")) | color(dim_txt),
        ".post-nav-next::after"_s | prop("content", raw("' && cd ./'")) | color(dim_txt),
        ".post-nav a"_s | color(dim_p) | font_size(12_px),
        ".post-nav a:hover"_s | color(phosphor),
        ".related-posts"_s | border_top(1_px, dashed, gray),
        ".related-posts h2"_s | color(dim_p),
        ".related-posts h2::before"_s | prop("content", raw("'# '")),
        ".related-posts li a"_s | color(phosphor) | font_size(12_px),
        ".related-posts li a:hover"_s | color(raw("#50ff80")),
        ".related-posts li a::before"_s | prop("content", raw("'-> '")),

        // ── Series nav — tree style ──
        ".series-nav"_s | border_color(gray) | border_left_color(dim_p) | bg(transparent),
        ".series-label"_s | color(dim_p),
        ".series-label::before"_s | prop("content", raw("'$ tree '")),
        ".series-list"_s | list_style(none) | padding_left(0_px),
        ".series-list li"_s | padding_left(16_px),
        ".series-list li::before"_s | prop("content", raw("'\\251C\\2500\\2500 '"))
                                    | color(dim_txt),
        ".series-list li:last-child::before"_s | prop("content", raw("'\\2514\\2500\\2500 '"))
                                               | color(dim_txt),
        ".series-list a"_s | color(phosphor),
        ".series-current"_s | color(phosphor) | font_weight(400),
        ".series-current::after"_s | prop("content", raw("' \\2190 you are here'"))
                                   | color(dim_txt) | font_size(11_px),

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

        // ── Footer — process info ──
        "footer"_s | border_top(1_px, dashed, gray) | color(dim_txt) | font_size(11_px),
        ".footer-links a"_s | color(dim_p),
        ".footer-links a:hover"_s | color(phosphor),

        // ── Breadcrumbs as path ──
        "nav.breadcrumb"_s | color(dim_p) | font_size(12_px),
        "nav.breadcrumb a"_s | color(dim_p),
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

        // ── Subtle flicker on the whole page ──
        keyframes("flicker",
            frame(raw("0%"))   | opacity(0.97),
            frame(raw("5%"))   | opacity(1.0),
            frame(raw("10%"))  | opacity(0.98),
            frame(raw("15%"))  | opacity(1.0),
            frame(raw("100%")) | opacity(1.0)
        ),
        "body"_s | prop("animation", raw("flicker 4s infinite")),

        // ── Selection ──
        "::selection"_s | bg(phosphor) | color(black)
    ),

    // ── Component overrides — full structural power ──
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
            auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::tm tm{}; gmtime_r(&now, &tm);
            char timebuf[32]; std::strftime(timebuf, sizeof(timebuf), "%H:%M:%S", &tm);
            auto pid = std::to_string(::getpid());

            return dom::footer(
                div(class_("container"),
                    p_(dom::raw(
                        "[" + pid + "] "
                        + std::string(timebuf)
                        + " - " +
                        (!ctx.site.footer.copyright.empty()
                            ? ctx.site.footer.copyright
                            : std::string("process exited with code 0")))),
                    when(!ctx.site.footer.links.empty(),
                        div(class_("footer-links"),
                            each(ctx.site.footer.links, [](const auto& l) {
                                return a(href(l.url), l.title); })))));
        },

        // ── ls -l style post listing with permissions ──
        .post_listing = [](const PostListing& props, const Ctx&, Children) {
            if (!props.post) return empty();
            const auto& p = *props.post;

            auto date_str = fmt_date_short(p.published);
            auto size_str = fmt_size(p.reading_time_minutes);

            // Build tag string
            std::string tags_str;
            for (const auto& t : p.tags) {
                if (!tags_str.empty()) tags_str += ' ';
                tags_str += "--" + t.get();
            }

            return article(class_("post-listing"),
                dom::raw(
                    "<span class='ls-perms'>-rw-r--r--</span> "
                    "<span class='ls-user'>root root</span> "
                    "<span class='ls-size'>" + size_str + "</span>"
                    "<span class='ls-date'>" + date_str + "</span>"),
                a(href("/post/" + p.slug.get()), p.title.get()),
                when(!tags_str.empty(),
                    span(class_("ls-tags"), tags_str)));
        },

        .index = [](const Index& props, const Ctx& ctx, Children) {
            if (!props.posts) return empty();
            return section(
                h2("> ls -lt ./posts/"),
                p_(dom::raw(
                    "<span style='color:#4a7a4a;font-size:11px'>"
                    "total " + std::to_string(props.posts->size())
                    + " &nbsp;&nbsp;# "
                    + std::to_string(props.posts->size()) + " posts, "
                    + std::to_string(props.posts->size() * 5) + " min read time"
                    + "</span>")),
                each(*props.posts, [&](const PostSummary& p) {
                    return ctx(PostListing{.post = &p});
                }));
        },
    }),
};

} // namespace loom::theme::builtin
