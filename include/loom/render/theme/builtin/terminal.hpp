#pragma once
#include "../theme_def.hpp"
#include "../../component.hpp"
#include <ctime>

namespace loom::theme::builtin
{
using namespace css;
using namespace ui;

inline const ThemeDef terminal = {
    .light = {{"#1a1a1a"}, {"#e8e8e8"}, {"#b0b0b0"}, {"#2e2e2e"}, {"#5fba7d"}},
    .dark  = {{"#1a1a1a"}, {"#e8e8e8"}, {"#b0b0b0"}, {"#2e2e2e"}, {"#5fba7d"}},
    .font  = {"ui-monospace,'SF Mono',SFMono-Regular,Menlo,Consolas,monospace"},
    .font_size = "14px",
    .max_width = "720px",
    .line_height = "1.6",
    .heading_weight = "600",
    .header_size = "32px",
    .corners = Corners::Sharp,
    .density = Density::Normal,
    .border_weight = BorderWeight::Thin,
    .nav_style = NavStyle::Default,
    .tag_style = TagStyle::Bordered,
    .link_style = LinkStyle::None,
    .code_style = CodeBlockStyle::Plain,
    .inline_code = InlineCodeStyle::Background,
    .quote_style = BlockquoteStyle::AccentBorder,
    .card_hover = CardHover::Border,
    .hr_style = HrStyle::Line,
    .table_style = TableStyle::Minimal,
    .sidebar_style = SidebarStyle::Clean,
    .post_nav = PostNavStyle::Default,
    .scrollbar = Scrollbar::Thin,
    .styles = sheet(
        // ── Hide dark toggle — both palettes are the same ──
        ".theme-toggle"_s | display(none),

        // ── Header ──
        "header"_s | border_bottom(1_px, solid, hex("#2e2e2e")),
        "header h1"_s | font_weight(700) | font_size(32_px) | letter_spacing(1_px) | margin_bottom(0_px),
        "header h1 a"_s | color(hex("#f0f0f0")) | text_decoration(none),
        "header h1 a:hover"_s | color(hex("#5fba7d")),
        ".site-description"_s | color(hex("#888888")) | font_size(13_px) | margin_top(4_px),
        "nav"_s | margin_top(12_px),
        "nav a"_s | color(hex("#888888")) | font_weight(400) | font_size(13_px),
        "nav a:hover"_s | color(hex("#5fba7d")),

        // ── Listings ──
        ".post-listing"_s | display(block) | padding(16_px, 0_px) | border_bottom(1_px, solid, hex("#262626")),
        ".post-listing:hover"_s | bg(transparent),
        ".post-listing > a"_s | color(hex("#eeeeee")) | font_weight(600) | font_size(15_px)
            | display(block) | line_height(num(1.35)),
        ".post-listing > a:hover"_s | color(hex("#5fba7d")),
        ".t-meta"_s | display(flex) | gap(12_px) | margin_top(4_px) | font_size(13_px) | color(hex("#888888")),
        ".post-listing .excerpt"_s | display(block) | margin_top(6_px) | font_size(13_px)
            | color(hex("#808080")) | line_height(num(1.5)),
        ".post-listing-meta"_s | display(none),
        ".post-listing .post-tags"_s | margin_top(8_px),

        // ── Section headings ──
        "h2"_s | border_bottom(none) | font_size(12_px)
               | color(hex("#5fba7d")) | font_weight(600) | text_transform(uppercase) | letter_spacing(2_px)
               | margin_top(0_px) | margin_bottom(12_px) | padding_bottom(0_px),

        // ── Tags ──
        root() | set("tag-bg", transparent) | set("tag-text", hex("#888888")) | set("tag-radius", css::raw("0"))
               | set("tag-hover-bg", hex("#5fba7d")) | set("tag-hover-text", hex("#1a1a1a")),
        ".tag"_s | border(1_px, solid, hex("#3a3a3a")) | padding(1_px, 7_px) | font_size(11_px)
                 | transition(css::raw("all 0.15s")),
        ".tag:hover"_s | border_color(hex("#5fba7d")),

        // ── Cards ──
        ".post-card"_s | bg(hex("#212121")) | border(1_px, solid, hex("#2e2e2e"))
                       | padding(18_px) | transition(css::raw("border-color 0.15s")),
        ".post-card a"_s | color(hex("#eeeeee")) | font_weight(600) | font_size(15_px),
        ".post-card a:hover"_s | color(hex("#5fba7d")),
        ".post-card:hover"_s | border_color(hex("#5fba7d")),
        ".post-card .date"_s | color(hex("#888888")) | margin_top(6_px),
        ".post-card .excerpt"_s | color(hex("#808080")) | font_size(13_px) | margin_top(6_px),

        // ── Post page ──
        ".post-full h1"_s | font_size(26_px) | font_weight(700)
                          | margin_bottom(8_px) | line_height(num(1.3)) | color(hex("#f0f0f0")),
        ".post-meta"_s | color(hex("#888888")) | font_size(13_px),
        ".heading-anchor"_s | opacity(0.2),
        ".post-content blockquote,.page-content blockquote"_s
            | border_left(3_px, solid, hex("#5fba7d")) | color(hex("#b0b0b0"))
            | bg(hex("#212121")) | padding(10_px, 16_px),
        ".post-content a,.page-content a"_s | color(hex("#5fba7d")),
        ".post-content a:hover,.page-content a:hover"_s | text_decoration(underline),
        ".post-content pre,.page-content pre"_s | bg(hex("#212121")) | border(1_px, solid, hex("#2e2e2e")),
        ".post-content :not(pre)>code,.page-content :not(pre)>code"_s | bg(hex("#262626"))
            | padding(2_px, 5_px),
        ".post-content h1,.post-content h2,.post-content h3,.post-content h4,.page-content h1,.page-content h2,.page-content h3,.page-content h4"_s
            | color(hex("#eeeeee")),
        ".post-content h2,.page-content h2"_s | font_size(20_px) | margin_top(28_px) | border_bottom(none),
        ".post-content h3,.page-content h3"_s | font_size(16_px),
        ".post-content hr,.page-content hr"_s | border_top(1_px, solid, hex("#2e2e2e")),
        ".post-content table th,.page-content table th"_s | bg(hex("#212121")),
        ".post-content table th,.post-content table td,.page-content table th,.page-content table td"_s
            | border_color(hex("#2e2e2e")),

        // ── Post nav ──
        ".post-nav"_s | border_top(1_px, solid, hex("#2e2e2e")) | margin_top(32_px),
        ".post-nav a"_s | color(hex("#888888")) | font_size(13_px),
        ".post-nav a:hover"_s | color(hex("#5fba7d")),
        ".related-posts"_s | border_top(1_px, solid, hex("#2e2e2e")),
        ".related-posts h2"_s | color(hex("#5fba7d")),
        ".related-posts li a"_s | color(hex("#d8d8d8")),
        ".related-posts li a:hover"_s | color(hex("#5fba7d")),

        // ── Sidebar ──
        ".sidebar"_s | font_size(13_px),
        ".widget h3"_s | font_size(11_px) | letter_spacing(2_px) | color(hex("#5fba7d"))
                       | border_bottom(1_px, solid, hex("#2e2e2e")) | padding_bottom(6_px),
        ".widget li a"_s | font_size(13_px) | color(hex("#d8d8d8")),
        ".widget li a:hover"_s | color(hex("#5fba7d")),
        ".widget .date"_s | color(hex("#888888")),
        ".widget p"_s | color(hex("#888888")),

        // ── Footer ──
        "footer"_s | border_top(1_px, solid, hex("#2e2e2e")) | color(hex("#888888")),
        ".footer-links a"_s | color(hex("#888888")),
        ".footer-links a:hover"_s | color(hex("#5fba7d")),

        // ── Chrome ──
        "nav.breadcrumb"_s | color(hex("#888888")),
        "nav.breadcrumb a"_s | color(hex("#888888")),
        "nav.breadcrumb a:hover"_s | color(hex("#5fba7d")),
        ".series-nav"_s | border_color(hex("#2e2e2e")) | border_left_color(hex("#5fba7d")) | bg(hex("#212121")),
        ".series-label"_s | color(hex("#888888")),
        ".series-list a"_s | color(hex("#5fba7d")),
        ".series-current"_s | color(hex("#eeeeee")),
        "::selection"_s | bg(hex("#5fba7d")) | color(hex("#1a1a1a"))
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
                        ctx(Nav{})),
                    when(s.layout.show_theme_toggle, ctx(ThemeToggle{}))));
        },

        .footer = [](const component::Footer&, const Ctx& ctx, Children) {
            return dom::footer(
                div(class_("container"),
                    p_(dom::raw(!ctx.site.footer.copyright.empty()
                        ? ctx.site.footer.copyright
                        : std::string("Powered by Loom"))),
                    when(!ctx.site.footer.links.empty(),
                        div(class_("footer-links"),
                            each(ctx.site.footer.links, [](const auto& l) {
                                return a(href(l.url), l.title); })))));
        },

        .post_full = [](const PostFull& props, const Ctx& ctx, Children ch) {
            if (!props.post) return empty();
            const auto& post = *props.post;
            const auto  pctx = props.context ? *props.context : PostContext{};
            return article(class_("post-full"),
                h1(post.title.get()),
                ctx(PostMeta{.post = props.post}),
                when(ctx.site.layout.show_post_tags && !post.tags.empty(),
                    div(class_("post-full post-tags"), ctx(TagList{.tags = &post.tags}))),
                ctx(SeriesNav{.series_name = pctx.series_name.get(), .posts = &pctx.series_posts, .current_slug = post.slug.get()}),
                ctx(PostContent{.post = props.post}),
                ctx(PostNav{.navigation = &pctx.nav}),
                ctx(RelatedPosts{.posts = &pctx.related}),
                fragment(std::move(ch)));
        },

        .post_listing = [](const PostListing& props, const Ctx& ctx, Children) {
            if (!props.post) return empty();
            const auto& p = *props.post;

            auto t = std::chrono::system_clock::to_time_t(p.published);
            std::tm tm{}; gmtime_r(&t, &tm);
            char datebuf[16]; std::strftime(datebuf, sizeof(datebuf), "%b %d", &tm);

            return article(class_("post-listing"),
                a(href("/post/" + p.slug.get()), p.title.get()),
                div(class_("t-meta"),
                    span(class_("date"), datebuf),
                    when(p.reading_time_minutes > 0,
                        span(class_("reading-time"), std::to_string(p.reading_time_minutes) + " min read"))),
                when(!p.excerpt.empty(),
                    p_(class_("excerpt"), p.excerpt)),
                when(!p.tags.empty(),
                    ctx(TagList{.tags = &p.tags})));
        },

        .post_card = [](const PostCard& props, const Ctx& ctx, Children) {
            if (!props.post) return empty();
            const auto& p = *props.post;

            auto t = std::chrono::system_clock::to_time_t(p.published);
            std::tm tm{}; gmtime_r(&t, &tm);
            char datebuf[16]; std::strftime(datebuf, sizeof(datebuf), "%b %d", &tm);

            return div(class_("post-card"),
                a(href("/post/" + p.slug.get()), p.title.get()),
                span(class_("date"), datebuf),
                when(!p.excerpt.empty(),
                    p_(class_("excerpt"), p.excerpt)),
                when(!p.tags.empty(),
                    ctx(TagList{.tags = &p.tags})));
        },

        .index = [](const Index& props, const Ctx& ctx, Children) {
            if (!props.posts) return empty();
            return section(
                h2("posts"),
                each(*props.posts, [&](const PostSummary& p) {
                    return ctx(PostListing{.post = &p});
                }));
        },
    }),
};

} // namespace loom::theme::builtin
