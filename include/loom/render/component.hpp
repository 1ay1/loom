#pragma once
// ═══════════════════════════════════════════════════════════════════════
//  component.hpp — JSX-like compile-time component system for Loom
//
//  Usage in main code:
//
//    auto ctx = Ctx::from(site);
//    auto html = ctx.page(meta, ctx(Index{.posts = &posts}), &sidebar).render();
//
//  Usage in themes — override any component's HTML:
//
//    using namespace dom;
//    using namespace component;
//
//    .components = overrides({
//        .header = [](const Header&, const Ctx& ctx, Children) {
//            return header(
//                div(class_("container header-bar"),
//                    div(class_("header-left"),
//                        h1(a(href("/"), ctx.site.title)),
//                        ctx(Nav{}))));
//        },
//    })
//
//  Call the default from inside an override:
//
//    .post_full = [](const PostFull& p, const Ctx& ctx, Children ch) {
//        // add a wrapper div, but render default inside it
//        return dom::div(class_("my-wrapper"), defaults(p, ctx, std::move(ch)));
//    }
//
// ═══════════════════════════════════════════════════════════════════════

#include <functional>
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <optional>
#include <chrono>
#include <type_traits>

#include "dom.hpp"
#include "../domain/site.hpp"
#include "../domain/post.hpp"
#include "../domain/post_summary.hpp"
#include "../domain/page.hpp"
#include "../engine/blog_engine.hpp"

namespace loom::theme { struct ThemeDef; }

namespace loom::component
{

using Node     = dom::Node;
using Children = std::vector<Node>;

struct Ctx;

// ─────────────────────────────────────────────────────────────────────
//  Render data
// ─────────────────────────────────────────────────────────────────────

struct PostNavigation
{
    std::optional<PostSummary> prev;
    std::optional<PostSummary> next;
};

struct TocEntry
{
    int level = 2;
    std::string id;
    std::string text;
};

std::vector<TocEntry> extract_toc(const std::string& html);

struct PostContext
{
    PostNavigation nav;
    std::vector<PostSummary> related;
    std::vector<PostSummary> series_posts;
    Series series_name{""};
    std::vector<TocEntry> toc;
};

struct PaginationInfo
{
    int current_page = 1;
    int total_pages = 1;
    bool has_prev() const { return current_page > 1; }
    bool has_next() const { return current_page < total_pages; }
    std::string prev_url() const {
        if (current_page == 2) return "/";
        return "/page/" + std::to_string(current_page - 1);
    }
    std::string next_url() const {
        return "/page/" + std::to_string(current_page + 1);
    }
};

struct SidebarData
{
    std::vector<PostSummary> recent_posts;
    std::vector<Tag> tags;
    std::vector<TagInfo> tag_infos;
    std::vector<Series> series;
    std::vector<SeriesInfo> series_infos;
};

// ─────────────────────────────────────────────────────────────────────
//  Components — each struct is props + a static default render
// ─────────────────────────────────────────────────────────────────────

struct Document        { const PageMeta* page_meta = nullptr; static Node render(const Document&, const Ctx&, Children); };
struct Head            { const PageMeta* page_meta = nullptr; static Node render(const Head&, const Ctx&, Children); };
struct Body            { static Node render(const Body&, const Ctx&, Children); };
struct Header          { static Node render(const Header&, const Ctx&, Children); };
struct Footer          { static Node render(const Footer&, const Ctx&, Children); };
struct Nav             { static Node render(const Nav&, const Ctx&, Children); };
struct ThemeToggle     { static Node render(const ThemeToggle&, const Ctx&, Children); };
struct Breadcrumbs     { std::string canonical_path, og_type, page_title; static Node render(const Breadcrumbs&, const Ctx&, Children); };
struct ContentArea     { bool has_sidebar = false; Node sidebar_node = {}; static Node render(const ContentArea&, const Ctx&, Children); };

struct Sidebar         { const SidebarData* data = nullptr; static Node render(const Sidebar&, const Ctx&, Children); };
struct Widget          { std::string heading; static Node render(const Widget&, const Ctx&, Children); };
struct RecentPostsWidget { const SidebarData* data = nullptr; static Node render(const RecentPostsWidget&, const Ctx&, Children); };
struct TagCloudWidget  { const SidebarData* data = nullptr; static Node render(const TagCloudWidget&, const Ctx&, Children); };
struct ArchivesWidget  { static Node render(const ArchivesWidget&, const Ctx&, Children); };
struct SeriesWidget    { const SidebarData* data = nullptr; static Node render(const SeriesWidget&, const Ctx&, Children); };
struct AboutWidget     { static Node render(const AboutWidget&, const Ctx&, Children); };

struct PostFull        { const Post* post = nullptr; const PostContext* context = nullptr; static Node render(const PostFull&, const Ctx&, Children); };
struct PostMeta        { const Post* post = nullptr; static Node render(const PostMeta&, const Ctx&, Children); };
struct PostContent     { const Post* post = nullptr; static Node render(const PostContent&, const Ctx&, Children); };
struct PostListing     { const PostSummary* post = nullptr; static Node render(const PostListing&, const Ctx&, Children); };
struct PostCard        { const PostSummary* post = nullptr; static Node render(const PostCard&, const Ctx&, Children); };
struct PostNav         { const PostNavigation* navigation = nullptr; static Node render(const PostNav&, const Ctx&, Children); };
struct RelatedPosts    { const std::vector<PostSummary>* posts = nullptr; static Node render(const RelatedPosts&, const Ctx&, Children); };
struct SeriesNav       { std::string series_name; const std::vector<PostSummary>* posts = nullptr; std::string current_slug; static Node render(const SeriesNav&, const Ctx&, Children); };
struct TagList         { const std::vector<Tag>* tags = nullptr; static Node render(const TagList&, const Ctx&, Children); };

struct Index           { const std::vector<PostSummary>* posts = nullptr; const std::vector<PostSummary>* featured = nullptr; const PaginationInfo* pagination = nullptr; static Node render(const Index&, const Ctx&, Children); };
struct FeaturedSection { const std::vector<PostSummary>* posts = nullptr; static Node render(const FeaturedSection&, const Ctx&, Children); };
struct Pagination      { PaginationInfo info; static Node render(const Pagination&, const Ctx&, Children); };
struct TagPage         { Tag tag{""}; const std::vector<PostSummary>* posts = nullptr; int post_count = 0; static Node render(const TagPage&, const Ctx&, Children); };
struct TagIndex        { const std::vector<TagInfo>* tag_infos = nullptr; static Node render(const TagIndex&, const Ctx&, Children); };
struct Archives        { const std::map<int, std::vector<PostSummary>, std::greater<int>>* by_year = nullptr; static Node render(const Archives&, const Ctx&, Children); };
struct SeriesPage      { Series series{""}; const std::vector<PostSummary>* posts = nullptr; static Node render(const SeriesPage&, const Ctx&, Children); };
struct SeriesIndex     { const std::vector<SeriesInfo>* all_series_info = nullptr; static Node render(const SeriesIndex&, const Ctx&, Children); };
struct PageView        { const Page* page = nullptr; std::vector<TocEntry> toc; static Node render(const PageView&, const Ctx&, Children); };
struct NotFound        { static Node render(const NotFound&, const Ctx&, Children); };
struct SearchPage      { const std::vector<PostSummary>* posts = nullptr; static Node render(const SearchPage&, const Ctx&, Children); };
struct TableOfContents { const std::vector<TocEntry>* entries = nullptr; static Node render(const TableOfContents&, const Ctx&, Children); };
struct ReadingProgress { static Node render(const ReadingProgress&, const Ctx&, Children); };
struct PostGraph       { const std::vector<PostSummary>* posts = nullptr; static Node render(const PostGraph&, const Ctx&, Children); };

// ─────────────────────────────────────────────────────────────────────
//  ComponentOverrides — one std::function slot per component
//
//  Theme authors set the slots they want to replace.
//  Empty slots (default) fall through to the component's static render.
// ─────────────────────────────────────────────────────────────────────

template<typename C>
using RenderFn = std::function<Node(const C&, const Ctx&, Children)>;

struct ComponentOverrides
{
    RenderFn<Document>   document{};    RenderFn<Head>            head{};
    RenderFn<Body>       body{};        RenderFn<Header>          header{};
    RenderFn<Footer>     footer{};      RenderFn<Nav>             nav{};
    RenderFn<ThemeToggle> theme_toggle{}; RenderFn<Breadcrumbs>   breadcrumbs{};
    RenderFn<ContentArea> content_area{};

    RenderFn<Sidebar>    sidebar{};     RenderFn<Widget>          widget{};
    RenderFn<RecentPostsWidget> recent_posts_widget{};
    RenderFn<TagCloudWidget>    tag_cloud_widget{};
    RenderFn<ArchivesWidget>    archives_widget{};
    RenderFn<SeriesWidget>      series_widget{};
    RenderFn<AboutWidget>       about_widget{};

    RenderFn<PostFull>   post_full{};   RenderFn<PostMeta>        post_meta{};
    RenderFn<PostContent> post_content{}; RenderFn<PostListing>   post_listing{};
    RenderFn<PostCard>   post_card{};   RenderFn<PostNav>         post_nav{};
    RenderFn<RelatedPosts> related_posts{}; RenderFn<SeriesNav>   series_nav{};
    RenderFn<TagList>    tag_list{};

    RenderFn<Index>      index{};       RenderFn<FeaturedSection> featured_section{};
    RenderFn<Pagination> pagination{};  RenderFn<TagPage>         tag_page{};
    RenderFn<TagIndex>   tag_index{};   RenderFn<Archives>        archives{};
    RenderFn<SeriesPage> series_page{}; RenderFn<SeriesIndex>     series_index{};
    RenderFn<PageView>   page_view{};   RenderFn<NotFound>        not_found{};
    RenderFn<SearchPage> search_page{};
    RenderFn<TableOfContents> table_of_contents{};
    RenderFn<ReadingProgress> reading_progress{};
    RenderFn<PostGraph>       post_graph{};

    // Type-safe lookup — resolves component type to the correct slot
    template<typename C>
    const RenderFn<C>& get() const
    {
             if constexpr (std::is_same_v<C, Document>)          return document;
        else if constexpr (std::is_same_v<C, Head>)              return head;
        else if constexpr (std::is_same_v<C, Body>)              return body;
        else if constexpr (std::is_same_v<C, Header>)            return header;
        else if constexpr (std::is_same_v<C, Footer>)            return footer;
        else if constexpr (std::is_same_v<C, Nav>)               return nav;
        else if constexpr (std::is_same_v<C, ThemeToggle>)       return theme_toggle;
        else if constexpr (std::is_same_v<C, Breadcrumbs>)       return breadcrumbs;
        else if constexpr (std::is_same_v<C, ContentArea>)       return content_area;
        else if constexpr (std::is_same_v<C, Sidebar>)           return sidebar;
        else if constexpr (std::is_same_v<C, Widget>)            return widget;
        else if constexpr (std::is_same_v<C, RecentPostsWidget>) return recent_posts_widget;
        else if constexpr (std::is_same_v<C, TagCloudWidget>)    return tag_cloud_widget;
        else if constexpr (std::is_same_v<C, ArchivesWidget>)    return archives_widget;
        else if constexpr (std::is_same_v<C, SeriesWidget>)      return series_widget;
        else if constexpr (std::is_same_v<C, AboutWidget>)       return about_widget;
        else if constexpr (std::is_same_v<C, PostFull>)          return post_full;
        else if constexpr (std::is_same_v<C, PostMeta>)          return post_meta;
        else if constexpr (std::is_same_v<C, PostContent>)       return post_content;
        else if constexpr (std::is_same_v<C, PostListing>)       return post_listing;
        else if constexpr (std::is_same_v<C, PostCard>)          return post_card;
        else if constexpr (std::is_same_v<C, PostNav>)           return post_nav;
        else if constexpr (std::is_same_v<C, RelatedPosts>)      return related_posts;
        else if constexpr (std::is_same_v<C, SeriesNav>)         return series_nav;
        else if constexpr (std::is_same_v<C, TagList>)           return tag_list;
        else if constexpr (std::is_same_v<C, Index>)             return index;
        else if constexpr (std::is_same_v<C, FeaturedSection>)   return featured_section;
        else if constexpr (std::is_same_v<C, Pagination>)        return pagination;
        else if constexpr (std::is_same_v<C, TagPage>)           return tag_page;
        else if constexpr (std::is_same_v<C, TagIndex>)          return tag_index;
        else if constexpr (std::is_same_v<C, Archives>)          return archives;
        else if constexpr (std::is_same_v<C, SeriesPage>)        return series_page;
        else if constexpr (std::is_same_v<C, SeriesIndex>)       return series_index;
        else if constexpr (std::is_same_v<C, PageView>)          return page_view;
        else if constexpr (std::is_same_v<C, NotFound>)          return not_found;
        else if constexpr (std::is_same_v<C, SearchPage>)        return search_page;
        else if constexpr (std::is_same_v<C, TableOfContents>)  return table_of_contents;
        else if constexpr (std::is_same_v<C, ReadingProgress>)  return reading_progress;
        else if constexpr (std::is_same_v<C, PostGraph>)       return post_graph;
    }
};

// ── Factory: wrap overrides in shared_ptr for ThemeDef ──
//
//   .components = overrides({ .header = ..., .footer = ... })
//
inline std::shared_ptr<ComponentOverrides> overrides(ComponentOverrides o)
{
    return std::make_shared<ComponentOverrides>(std::move(o));
}

// ── Call the default render from inside an override ──
//
//   .post_full = [](const PostFull& p, const Ctx& ctx, Children ch) {
//       return div(class_("wrapper"), defaults(p, ctx, std::move(ch)));
//   }
//
template<typename C>
Node defaults(const C& props, const Ctx& ctx, Children ch)
{
    return C::render(props, ctx, std::move(ch));
}

// ─────────────────────────────────────────────────────────────────────
//  Ctx — the render context
// ─────────────────────────────────────────────────────────────────────

namespace detail
{
inline void collect(Children& out, Node&& n)              { out.push_back(std::move(n)); }
inline void collect(Children& out, const Node& n)         { out.push_back(n); }
inline void collect(Children& out, Children&& nodes)      { for (auto& n : nodes) out.push_back(std::move(n)); }
inline void collect(Children& out, const Children& nodes) { for (const auto& n : nodes) out.push_back(n); }
inline void collect(Children& out, const std::string& s)  { out.push_back(dom::text(s)); }
inline void collect(Children& out, const char* s)         { out.push_back(dom::text(s)); }
} // namespace detail

struct Ctx
{
    const Site& site;
    const ComponentOverrides* overrides = nullptr;
    const theme::ThemeDef* theme_def = nullptr;
    const AssetManifest* assets = nullptr;

    // ctx(Component{.props}, child1, child2, ...)
    template<typename C, typename... Args>
    Node operator()(const C& component, Args&&... children) const
    {
        Children ch;
        if constexpr (sizeof...(Args) > 0)
        {
            ch.reserve(sizeof...(Args));
            (detail::collect(ch, std::forward<Args>(children)), ...);
        }

        using Comp = std::decay_t<C>;
        if (overrides)
        {
            const auto& fn = overrides->get<Comp>();
            if (fn) return fn(component, *this, std::move(ch));
        }
        return Comp::render(component, *this, std::move(ch));
    }

    // Render a full page with the standard layout
    Node page(const PageMeta& meta, Node content,
              const SidebarData* sidebar_data = nullptr) const;

    // Render component to string
    template<typename C, typename... Args>
    std::string render(const C& component, Args&&... children) const
    {
        return operator()(component, std::forward<Args>(children)...).render();
    }

    // Build from site — resolves theme overrides automatically
    static Ctx from(const Site& site, const AssetManifest* assets = nullptr);
};

// ── Build the full combined CSS for the site ──
// Used by the cache builder to create the external CSS asset.
std::string build_combined_css(const Site& site);

// ── Build the full combined UX JS bundle ──
// Used by the cache builder to create the external JS asset.
std::string build_ux_js_bundle();

// ── Helper: fragment from children vector ──

inline Node fragment(Children ch)
{
    Node f;
    f.kind = Node::Fragment;
    f.children = std::move(ch);
    return f;
}

} // namespace loom::component

// ─────────────────────────────────────────────────────────────────────
//  Theme authoring namespace — one `using namespace ui;` gives theme
//  authors access to all component types, dom elements, and helpers
//  without clashing with css::raw.
//
//  Usage in theme files:
//    using namespace ui;
//    .components = overrides({
//        .header = [](const Header&, const Ctx& ctx, Children) {
//            return header(div(class_("container header-bar"), ...));
//        },
//    })
// ─────────────────────────────────────────────────────────────────────
namespace loom::ui
{
// Component types + helpers
using component::Ctx;
using component::Node;
using component::Children;
using component::overrides;
using component::defaults;
using component::fragment;
using component::PostContext;
using component::PostNavigation;
using component::SidebarData;
using component::PaginationInfo;

// All component structs
using component::Document;
using component::Head;
using component::Body;
using component::Header;
// Footer not re-exported — clashes with loom::Footer domain type.
// Use component::Footer in lambda signatures.
using component::Nav;
using component::ThemeToggle;
using component::Breadcrumbs;
using component::ContentArea;
using component::Sidebar;
using component::Widget;
using component::RecentPostsWidget;
using component::TagCloudWidget;
using component::ArchivesWidget;
using component::SeriesWidget;
using component::AboutWidget;
using component::PostFull;
using component::PostMeta;
using component::PostContent;
using component::PostListing;
using component::PostCard;
using component::PostNav;
using component::RelatedPosts;
using component::SeriesNav;
using component::TagList;
using component::Index;
using component::FeaturedSection;
using component::Pagination;
using component::TagPage;
using component::TagIndex;
using component::Archives;
using component::SeriesPage;
using component::SeriesIndex;
using component::PageView;
using component::NotFound;
using component::SearchPage;
using component::TableOfContents;
using component::ReadingProgress;
using component::PostGraph;
using component::TocEntry;

// DOM elements (the ones theme authors actually use)
using dom::div;
using dom::span;
using dom::p_;
using dom::h1;
using dom::h2;
using dom::h3;
using dom::h4;
using dom::h5;
using dom::h6;
using dom::a;
using dom::ul;
using dom::ol;
using dom::li;
using dom::article;
using dom::section;
using dom::aside;
using dom::main_;
using dom::button;
using dom::hr;
using dom::img;
using dom::strong;
using dom::em_;
using dom::pre;
using dom::code;
using dom::time_;
using dom::when;
using dom::unless;
using dom::each;
using dom::each_i;
using dom::empty;
using dom::text;

// DOM attributes
using dom::class_;
using dom::id;
using dom::href;
using dom::src;
using dom::alt;
using dom::onclick;
using dom::aria_label;
using dom::classes;
using dom::attr;

// These need explicit namespace because they clash:
//   dom::raw    — use dom::raw() for HTML
//   dom::header — use dom::header() for <header> element
//   dom::footer — use dom::footer() for <footer> element
//   dom::nav    — use dom::nav() for <nav> element
// (Header/Footer/Nav as unqualified names refer to component types)

} // namespace loom::ui
