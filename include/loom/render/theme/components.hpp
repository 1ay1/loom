#pragma once

namespace loom::theme
{

// Shape language — controls border-radius across all elements
enum class Corners { Soft, Sharp, Round };

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

} // namespace loom::theme
