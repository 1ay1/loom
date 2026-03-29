#include "../../include/loom/util/minify.hpp"

#include <cstring>

namespace loom
{

// ── CSS minifier — strip comments and collapse whitespace ──
// Safe for use on already-compact CSS from the DSL (handles edge
// cases like url() data URIs and quoted strings).
static std::string minify_css(const std::string& css)
{
    std::string out;
    out.reserve(css.size());

    size_t i = 0;
    size_t len = css.size();

    while (i < len)
    {
        // Skip CSS comments: /* ... */
        if (i + 1 < len && css[i] == '/' && css[i + 1] == '*')
        {
            i += 2;
            while (i + 1 < len && !(css[i] == '*' && css[i + 1] == '/'))
                ++i;
            if (i + 1 < len) i += 2;
            continue;
        }

        // Preserve quoted strings verbatim
        if (css[i] == '"' || css[i] == '\'')
        {
            char q = css[i];
            out += css[i++];
            while (i < len && css[i] != q)
            {
                if (css[i] == '\\' && i + 1 < len) { out += css[i++]; }
                out += css[i++];
            }
            if (i < len) out += css[i++];
            continue;
        }

        // Collapse whitespace
        if (css[i] == ' ' || css[i] == '\t' || css[i] == '\n' || css[i] == '\r')
        {
            while (i < len && (css[i] == ' ' || css[i] == '\t' || css[i] == '\n' || css[i] == '\r'))
                ++i;

            // Only keep space where it's syntactically required:
            // between identifiers/values (not around : ; { } , > + ~ )
            if (!out.empty() && i < len)
            {
                char prev = out.back();
                char next = css[i];
                bool prev_is_punct = (prev == '{' || prev == '}' || prev == ';' || prev == ':'
                    || prev == ',' || prev == '>' || prev == '+' || prev == '~'
                    || prev == '(' || prev == ')');
                bool next_is_punct = (next == '{' || next == '}' || next == ';' || next == ':'
                    || next == ',' || next == '>' || next == '+' || next == '~'
                    || next == '(' || next == ')');
                if (!prev_is_punct && !next_is_punct)
                    out += ' ';
            }
            continue;
        }

        out += css[i++];
    }

    return out;
}

std::string minify_html(const std::string& html)
{
    std::string out;
    out.reserve(html.size());

    size_t i = 0;
    size_t len = html.size();

    auto starts_with = [&](size_t pos, const char* prefix) {
        return html.compare(pos, std::strlen(prefix), prefix) == 0;
    };

    // Skip past closing tag: finds "</tag>" or "</tag " case-insensitively
    auto skip_past_close = [&](size_t pos, const char* tag) {
        size_t tag_len = std::strlen(tag);
        while (pos < len)
        {
            if (html[pos] == '<' && pos + 1 < len && html[pos + 1] == '/')
            {
                bool match = true;
                for (size_t j = 0; j < tag_len && pos + 2 + j < len; ++j)
                {
                    char a = html[pos + 2 + j];
                    char b = tag[j];
                    if (a >= 'A' && a <= 'Z') a += 32;
                    if (b >= 'A' && b <= 'Z') b += 32;
                    if (a != b) { match = false; break; }
                }
                if (match && pos + 2 + tag_len < len && html[pos + 2 + tag_len] == '>')
                    return pos + 2 + tag_len + 1;
            }
            ++pos;
        }
        return pos;
    };

    while (i < len)
    {
        if (html[i] == '<')
        {
            // Preserve <pre>, <script>, <textarea> verbatim
            const char* preserve_tag = nullptr;
            if (starts_with(i, "<pre") && (html[i+4] == '>' || html[i+4] == ' '))
                preserve_tag = "pre";
            else if (starts_with(i, "<script") && (html[i+7] == '>' || html[i+7] == ' '))
                preserve_tag = "script";
            else if (starts_with(i, "<textarea") && (html[i+9] == '>' || html[i+9] == ' '))
                preserve_tag = "textarea";

            if (preserve_tag)
            {
                size_t end = skip_past_close(i, preserve_tag);
                out.append(html, i, end - i);
                i = end;
                continue;
            }

            // Minify inline <style> content
            if (starts_with(i, "<style") && (html[i+6] == '>' || html[i+6] == ' '))
            {
                size_t end = skip_past_close(i, "style");
                // Find the content between <style...> and </style>
                size_t tag_end = html.find('>', i);
                if (tag_end != std::string::npos && tag_end < end)
                {
                    size_t content_start = tag_end + 1;
                    // Find </style> within the block
                    size_t close_start = end - 8; // length of "</style>"
                    // Extract and minify the CSS content
                    out.append(html, i, content_start - i); // <style...>
                    std::string css_content(html, content_start, close_start - content_start);
                    out += minify_css(css_content);
                    out += "</style>";
                }
                else
                {
                    out.append(html, i, end - i);
                }
                i = end;
                continue;
            }
        }

        // Collapse runs of whitespace between/around tags to a single space
        if (html[i] == ' ' || html[i] == '\t' || html[i] == '\n' || html[i] == '\r')
        {
            // Skip all whitespace
            while (i < len && (html[i] == ' ' || html[i] == '\t' || html[i] == '\n' || html[i] == '\r'))
                ++i;

            // Omit whitespace entirely if it's between tags: >   <
            if (!out.empty() && out.back() == '>' && i < len && html[i] == '<')
                continue;

            // Otherwise collapse to single space
            out += ' ';
            continue;
        }

        out += html[i];
        ++i;
    }

    return out;
}

// Standalone CSS minifier for external CSS files
std::string minify_css_standalone(const std::string& css)
{
    return minify_css(css);
}

}
