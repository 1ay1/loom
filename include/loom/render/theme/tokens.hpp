#pragma once

namespace loom::theme
{

// Color tokens — each carries its CSS custom property name
struct Bg     { static constexpr auto var = "--bg"; };
struct Text   { static constexpr auto var = "--text"; };
struct Muted  { static constexpr auto var = "--muted"; };
struct Border { static constexpr auto var = "--border"; };
struct Accent { static constexpr auto var = "--accent"; };

// Typography tokens
struct Font     { static constexpr auto var = "--font"; };
struct FontSize { static constexpr auto var = "--font-size"; };
struct MaxWidth { static constexpr auto var = "--max-width"; };

} // namespace loom::theme
