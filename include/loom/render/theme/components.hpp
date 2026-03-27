#pragma once

namespace loom::theme
{

// ── Shape ──────────────────────────────────────────────

// Border-radius across all elements
enum class Corners { Soft, Sharp, Round };

// Structural border thickness (header, footer, sidebar, cards)
enum class BorderWeight { Thin, Normal, Thick };

// ── Spacing ────────────────────────────────────────────

// Site-wide spacing rhythm: line-height, margins, padding
enum class Density { Compact, Normal, Airy };

// ── Navigation ─────────────────────────────────────────

// Navigation link treatment
enum class NavStyle {
    Default,   // plain links, muted color, accent on hover
    Pills,     // background pill on hover
    Underline, // bottom border on hover
    Minimal    // smaller font, tighter spacing
};

// ── Tags ───────────────────────────────────────────────

enum class TagStyle {
    Pill,       // rounded bg, no border (default)
    Rect,       // square bg, no border
    Bordered,   // transparent, accent-colored border
    Outline,    // transparent, muted border, accent on hover
    Plain       // no bg, no border, just text
};

// ── Content links ──────────────────────────────────────

enum class LinkStyle {
    Underline,  // solid underline (default)
    Dotted,     // dotted underline, solid on hover
    Dashed,     // dashed underline
    None        // no decoration, color only
};

// ── Code ───────────────────────────────────────────────

// Fenced code blocks
enum class CodeBlockStyle {
    Plain,      // background only (default)
    Bordered,   // background + full border
    LeftAccent  // background + left accent-colored border
};

// Inline `code` spans
enum class InlineCodeStyle {
    Background, // bg tint (default)
    Bordered,   // bg tint + border
    Plain       // no bg, no border
};

// ── Blockquotes ────────────────────────────────────────

enum class BlockquoteStyle {
    AccentBorder, // left border in accent color (default)
    MutedBorder   // left border in muted color
};

// ── Headings ───────────────────────────────────────────

enum class HeadingCase {
    None,  // as written (default)
    Upper, // UPPERCASE + letter-spacing
    Lower  // lowercase
};

// ── Images ─────────────────────────────────────────────

enum class ImageStyle {
    Default,  // just border-radius from Corners
    Bordered, // thin border in border color
    Shadow    // subtle drop shadow
};

// ── Cards ──────────────────────────────────────────────

enum class CardHover {
    Lift,   // translate up + shadow (default)
    Border, // border-color only, no motion
    Glow,   // accent-tinted glow shadow
    None    // no hover effect
};

// ── Horizontal rules ───────────────────────────────────

enum class HrStyle {
    Line,   // solid 1px border (default)
    Dashed, // dashed border
    Fade    // gradient fade to transparent
};

// ── Tables ─────────────────────────────────────────────

enum class TableStyle {
    Default,  // header bg + full borders
    Striped,  // alternating row backgrounds
    Bordered, // thicker borders, strong header
    Minimal   // header underline only, no cell borders
};

// ── Sidebar ────────────────────────────────────────────

enum class SidebarStyle {
    Bordered, // vertical border between content and sidebar (default)
    Clean,    // no border, just gap
    Card      // widgets in bordered cards
};

// ── Post navigation (prev/next) ────────────────────────

enum class PostNavStyle {
    Default,  // accent links, top border
    Arrows,   // larger, bolder with arrow emphasis
    Minimal   // smaller, muted
};

// ── Scrollbar ──────────────────────────────────────────

enum class Scrollbar {
    Default, // browser default
    Thin,    // thin scrollbar, themed colors
    Hidden   // hidden (scroll still works)
};

// ── Focus ring ─────────────────────────────────────────

enum class FocusStyle {
    Outline, // 2px accent outline (default)
    Ring,    // thicker ring with offset
    None     // no visible focus (accessibility concern)
};

} // namespace loom::theme
