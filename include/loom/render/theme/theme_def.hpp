#pragma once

#include <memory>
#include <string>
#include "palette.hpp"
#include "types.hpp"
#include "components.hpp"
#include "css.hpp"

// Forward-declare to avoid pulling component.hpp into every theme header.
// The full definition lives in loom/render/component.hpp.
namespace loom::component { struct ComponentOverrides; }

namespace loom::theme
{

// Complete theme definition — typed fields across 6 axes.
//
// Only the first 5 fields (light, dark, font, font_size, max_width) need
// to be specified. Everything else defaults to the base CSS behavior.
//
// The `components` field enables WordPress-style structural overrides:
// themes can replace the HTML of any component (Header, Footer, PostCard,
// etc.) while the rest falls back to Loom's built-in defaults.
struct ThemeDef
{
    // ── Colors ──
    Palette   light;
    Palette   dark;

    // ── Typography ──
    FontStack   font;
    std::string font_size;
    std::string max_width;
    FontStack   heading_font   = {};   // empty = inherit
    FontStack   code_font      = {};   // empty = base CSS monospace
    std::string line_height    = {};   // empty = "1.7"
    std::string heading_weight = {};   // empty = "700"
    std::string header_size    = {};   // empty = "42px"

    // ── Shape & density ──
    Corners      corners      = Corners::Soft;
    Density      density      = Density::Normal;
    BorderWeight border_weight = BorderWeight::Normal;

    // ── Component styles (CSS-level) ──
    NavStyle        nav_style    = NavStyle::Default;
    TagStyle        tag_style    = TagStyle::Pill;
    LinkStyle       link_style   = LinkStyle::Underline;
    CodeBlockStyle  code_style   = CodeBlockStyle::Plain;
    InlineCodeStyle inline_code  = InlineCodeStyle::Background;
    BlockquoteStyle quote_style  = BlockquoteStyle::AccentBorder;
    HeadingCase     heading_case = HeadingCase::None;
    ImageStyle      image_style  = ImageStyle::Default;
    CardHover       card_hover   = CardHover::Lift;
    HrStyle         hr_style     = HrStyle::Line;
    TableStyle      table_style  = TableStyle::Default;
    SidebarStyle    sidebar_style = SidebarStyle::Bordered;
    PostNavStyle    post_nav     = PostNavStyle::Default;
    Scrollbar       scrollbar    = Scrollbar::Default;
    FocusStyle      focus_style  = FocusStyle::Outline;

    // ── Custom styles (typed CSS DSL) ──
    css::Sheet styles = {};

    // ── Raw escape hatch (prefer styles above) ──
    std::string extra_css = {};

    // ── Component overrides (structural / HTML-level) ──
    //
    // WordPress-style: replace any component's render function.
    // Uses shared_ptr so ThemeDef stays copyable/movable without
    // pulling in the full ComponentOverrides definition.
    //
    //   #include "loom/render/component.hpp"
    //
    //   .components = std::make_shared<component::ComponentOverrides>(
    //       component::ComponentOverrides{
    //           .header = [](const component::Header&, const component::Ctx& ctx,
    //                        component::Children ch) -> dom::Node {
    //               using namespace dom;
    //               return header(class_("custom"), h1(ctx.site.title));
    //           }
    //       })
    //
    std::shared_ptr<component::ComponentOverrides> components = {};
};

// Derive a new theme by copying a base and applying overrides in-place
template<typename F>
ThemeDef derive(ThemeDef base, F&& overrides)
{
    overrides(base);
    return base;
}

} // namespace loom::theme
