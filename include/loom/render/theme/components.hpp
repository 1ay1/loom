#pragma once

namespace loom::theme
{

// --- Shape ---

// Controls border-radius across all elements
enum class Corners { Soft, Sharp, Round };

// --- Spacing ---

// Controls line-height, margins, padding site-wide
enum class Density { Compact, Normal, Airy };

// --- Component styles ---

// Tag badge appearance
enum class TagStyle {
    Pill,       // rounded bg, no border (default)
    Rect,       // square bg, no border
    Bordered,   // transparent, accent-colored border
    Outline,    // transparent, muted border, accent on hover
    Plain       // no bg, no border, just text
};

// Content link decoration
enum class LinkStyle {
    Underline,  // solid underline (default)
    Dotted,     // dotted underline, solid on hover
    Dashed,     // dashed underline
    None        // no decoration, color only
};

// Code block treatment
enum class CodeBlockStyle {
    Plain,      // background only (default)
    Bordered,   // background + full border
    LeftAccent  // background + left accent-colored border
};

// Blockquote treatment
enum class BlockquoteStyle {
    AccentBorder, // left border in accent color (default)
    MutedBorder   // left border in muted color
};

// Heading text transform
enum class HeadingCase {
    None,  // as written (default)
    Upper, // UPPERCASE + letter-spacing
    Lower  // lowercase
};

// Image treatment in content
enum class ImageStyle {
    Default,  // just border-radius from Corners
    Bordered, // thin border in border color
    Shadow    // subtle drop shadow
};

// Post card hover interaction
enum class CardHover {
    Lift,   // translate up + shadow (default)
    Border, // border-color only, no motion
    Glow,   // accent-tinted glow shadow
    None    // no hover effect
};

// Horizontal rule style
enum class HrStyle {
    Line,   // solid 1px border (default)
    Dashed, // dashed border
    Fade    // gradient fade from transparent to border to transparent
};

// Table treatment
enum class TableStyle {
    Default,  // header bg + full borders (default)
    Striped,  // alternating row backgrounds
    Bordered, // thicker borders, strong header
    Minimal   // header underline only, no cell borders
};

} // namespace loom::theme
