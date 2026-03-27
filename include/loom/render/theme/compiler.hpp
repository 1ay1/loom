#pragma once

#include <string>
#include <tuple>
#include "tokens.hpp"
#include "palette.hpp"
#include "theme_def.hpp"

namespace loom::theme
{

namespace detail
{

// Binds a semantic token type to its Palette accessor via pointer-to-member.
// The fold expression in emit_palette() iterates over these at compile time.
template<typename Token, Color Palette::* Member>
struct ColorBinding
{
    static void emit(std::string& css, const Palette& p)
    {
        css += Token::var;
        css += ':';
        css += (p.*Member).value;
        css += ';';
    }
};

// Single source of truth: token <-> palette field mapping
using ColorBindings = std::tuple<
    ColorBinding<Bg,     &Palette::bg>,
    ColorBinding<Text,   &Palette::text>,
    ColorBinding<Muted,  &Palette::muted>,
    ColorBinding<Border, &Palette::border>,
    ColorBinding<Accent, &Palette::accent>
>;

template<typename... Bs>
void emit_palette(std::string& css, const Palette& p, std::tuple<Bs...>)
{
    (Bs::emit(css, p), ...);
}

} // namespace detail

// Theme -> CSS compiler
// Produces:  :root{...} [data-theme="dark"]{...} <extra_css>
inline std::string compile(const ThemeDef& t)
{
    std::string css;

    // Light mode: colors + typography
    css += ":root{";
    detail::emit_palette(css, t.light, detail::ColorBindings{});
    css += Font::var;     css += ':'; css += t.font.value;  css += ';';
    css += FontSize::var; css += ':'; css += t.font_size;   css += ';';
    css += MaxWidth::var; css += ':'; css += t.max_width;   css += ';';
    css += '}';

    // Dark mode: colors only
    css += "[data-theme=\"dark\"]{";
    detail::emit_palette(css, t.dark, detail::ColorBindings{});
    css += '}';

    // Escape hatch
    if (!t.extra_css.empty())
        css += t.extra_css;

    return css;
}

} // namespace loom::theme
