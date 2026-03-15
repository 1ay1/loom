#include "../../include/loom/util/minify.hpp"

#include <cstring>

namespace loom
{

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
        // Preserve content of <pre>, <script>, <style>, <code> verbatim
        if (html[i] == '<')
        {
            const char* preserve_tag = nullptr;
            if (starts_with(i, "<pre") && (html[i+4] == '>' || html[i+4] == ' '))
                preserve_tag = "pre";
            else if (starts_with(i, "<script") && (html[i+7] == '>' || html[i+7] == ' '))
                preserve_tag = "script";
            else if (starts_with(i, "<style") && (html[i+6] == '>' || html[i+6] == ' '))
                preserve_tag = "style";
            else if (starts_with(i, "<textarea") && (html[i+9] == '>' || html[i+9] == ' '))
                preserve_tag = "textarea";

            if (preserve_tag)
            {
                size_t end = skip_past_close(i, preserve_tag);
                out.append(html, i, end - i);
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

}
