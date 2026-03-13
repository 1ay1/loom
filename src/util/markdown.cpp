#include "../../include/loom/util/markdown.hpp"

#include <sstream>
#include <vector>

namespace loom
{

static std::string escape_html(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s)
    {
        switch (c)
        {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            default:   out += c;
        }
    }
    return out;
}

static std::string process_inline(const std::string& line)
{
    std::string out;
    out.reserve(line.size());

    for (size_t i = 0; i < line.size(); i++)
    {
        // Bold: **text**
        if (i + 1 < line.size() && line[i] == '*' && line[i + 1] == '*')
        {
            auto end = line.find("**", i + 2);
            if (end != std::string::npos)
            {
                out += "<strong>" + line.substr(i + 2, end - i - 2) + "</strong>";
                i = end + 1;
                continue;
            }
        }

        // Italic: *text*
        if (line[i] == '*' && (i + 1 < line.size()) && line[i + 1] != '*')
        {
            auto end = line.find('*', i + 1);
            if (end != std::string::npos)
            {
                out += "<em>" + line.substr(i + 1, end - i - 1) + "</em>";
                i = end;
                continue;
            }
        }

        // Inline code: `code`
        if (line[i] == '`')
        {
            auto end = line.find('`', i + 1);
            if (end != std::string::npos)
            {
                out += "<code>" + escape_html(line.substr(i + 1, end - i - 1)) + "</code>";
                i = end;
                continue;
            }
        }

        // Links: [text](url)
        if (line[i] == '[')
        {
            auto close_bracket = line.find(']', i + 1);
            if (close_bracket != std::string::npos &&
                close_bracket + 1 < line.size() &&
                line[close_bracket + 1] == '(')
            {
                auto close_paren = line.find(')', close_bracket + 2);
                if (close_paren != std::string::npos)
                {
                    auto text = line.substr(i + 1, close_bracket - i - 1);
                    auto url = line.substr(close_bracket + 2, close_paren - close_bracket - 2);
                    out += "<a href=\"" + url + "\">" + text + "</a>";
                    i = close_paren;
                    continue;
                }
            }
        }

        out += line[i];
    }

    return out;
}

std::string markdown_to_html(const std::string& markdown)
{
    std::string html;
    std::istringstream stream(markdown);
    std::string line;

    bool in_code_block = false;
    bool in_list = false;
    std::vector<std::string> paragraph;

    auto flush_paragraph = [&]() {
        if (paragraph.empty()) return;

        std::string text;
        for (size_t i = 0; i < paragraph.size(); i++)
        {
            if (i > 0) text += " ";
            text += paragraph[i];
        }
        html += "<p>" + process_inline(text) + "</p>\n";
        paragraph.clear();
    };

    auto close_list = [&]() {
        if (in_list)
        {
            html += "</ul>\n";
            in_list = false;
        }
    };

    while (std::getline(stream, line))
    {
        // Remove trailing \r
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        // Code blocks: ```
        if (line.substr(0, 3) == "```")
        {
            if (!in_code_block)
            {
                flush_paragraph();
                close_list();
                in_code_block = true;
                html += "<pre><code>";
            }
            else
            {
                in_code_block = false;
                html += "</code></pre>\n";
            }
            continue;
        }

        if (in_code_block)
        {
            html += escape_html(line) + "\n";
            continue;
        }

        // Empty line: flush paragraph
        if (line.empty())
        {
            flush_paragraph();
            close_list();
            continue;
        }

        // Headings: # ## ### etc.
        if (line[0] == '#')
        {
            flush_paragraph();
            close_list();

            int level = 0;
            while (level < (int)line.size() && line[level] == '#') level++;
            if (level > 6) level = 6;

            auto text = line.substr(level);
            if (!text.empty() && text[0] == ' ') text = text.substr(1);

            html += "<h" + std::to_string(level) + ">" + process_inline(text) + "</h" + std::to_string(level) + ">\n";
            continue;
        }

        // Horizontal rule: --- or ***
        if ((line.substr(0, 3) == "---" || line.substr(0, 3) == "***") &&
            line.find_first_not_of(line[0]) == std::string::npos)
        {
            flush_paragraph();
            close_list();
            html += "<hr>\n";
            continue;
        }

        // Unordered list: - item or * item
        if (line.size() > 2 && (line[0] == '-' || line[0] == '*') && line[1] == ' ')
        {
            flush_paragraph();
            if (!in_list)
            {
                html += "<ul>\n";
                in_list = true;
            }
            html += "<li>" + process_inline(line.substr(2)) + "</li>\n";
            continue;
        }

        // Regular text: accumulate paragraph
        paragraph.push_back(line);
    }

    flush_paragraph();
    close_list();

    if (in_code_block)
        html += "</code></pre>\n";

    return html;
}

}
