#include "../../include/loom/util/markdown.hpp"

#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <functional>

namespace loom
{

// ============================================================================
// Types
// ============================================================================

struct RefDef
{
    std::string url;
    std::string title;
};

using RefMap = std::unordered_map<std::string, RefDef>;

struct FootnoteDef
{
    std::string id;
    std::string content; // raw markdown lines
};

// ============================================================================
// String utilities
// ============================================================================

static std::string to_lower(const std::string& s)
{
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return r;
}

static std::string trim(const std::string& s)
{
    auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    auto b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static std::string ltrim(const std::string& s)
{
    auto a = s.find_first_not_of(" \t");
    if (a == std::string::npos) return "";
    return s.substr(a);
}

static size_t leading_spaces(const std::string& s)
{
    size_t n = 0;
    for (char c : s)
    {
        if (c == ' ') n++;
        else if (c == '\t') n += 4;
        else break;
    }
    return n;
}

static std::string strip_leading(const std::string& s, size_t count)
{
    size_t i = 0, removed = 0;
    while (i < s.size() && removed < count)
    {
        if (s[i] == ' ') { removed++; i++; }
        else if (s[i] == '\t') { removed += 4; i++; }
        else break;
    }
    return s.substr(i);
}

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

static bool is_punctuation(char c)
{
    // Lookup table for ASCII punctuation (CommonMark spec)
    static constexpr bool table[128] = {
        // 0-31: control chars
        0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
        // 32-47: space ! " # $ % & ' ( ) * + , - . /
        0,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
        // 48-63: 0-9 : ; < = > ?
        0,0,0,0,0,0,0,0, 0,0,1,1,1,1,1,1,
        // 64-79: @ A-O
        1,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
        // 80-95: P-Z [ \ ] ^ _
        0,0,0,0,0,0,0,0, 0,0,0,1,1,1,1,1,
        // 96-111: ` a-o
        1,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
        // 112-127: p-z { | } ~
        0,0,0,0,0,0,0,0, 0,0,0,1,1,1,1,0,
    };
    auto uc = static_cast<unsigned char>(c);
    return uc < 128 && table[uc];
}

static std::string slugify(const std::string& text)
{
    std::string slug;
    slug.reserve(text.size());
    bool prev_dash = true; // true suppresses leading dash
    for (char c : text)
    {
        auto uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc))
        {
            slug += static_cast<char>(std::tolower(uc));
            prev_dash = false;
        }
        else if (!prev_dash)
        {
            slug += '-';
            prev_dash = true;
        }
    }
    if (!slug.empty() && slug.back() == '-') slug.pop_back();
    return slug;
}

static std::string make_heading_id(const std::string& text,
                                   std::unordered_map<std::string, int>& seen)
{
    auto slug = slugify(text);
    if (slug.empty()) slug = "heading";
    auto it = seen.find(slug);
    if (it == seen.end())
    {
        seen[slug] = 1;
        return slug;
    }
    auto id = slug + "-" + std::to_string(it->second);
    it->second++;
    return id;
}

static std::vector<std::string> split_lines(const std::string& text)
{
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

// ============================================================================
// Inline processing
// ============================================================================

static std::string process_inline(const std::string& text, const RefMap& refs);

// Parse a link destination and optional title from (url "title") or (url)
static bool parse_link_dest(const std::string& text, size_t start, size_t& end,
                            std::string& url, std::string& title)
{
    if (start >= text.size() || text[start] != '(') return false;

    size_t i = start + 1;
    // Skip whitespace
    while (i < text.size() && text[i] == ' ') i++;

    // Find URL (scan to space, quote, or close paren)
    size_t url_start = i;
    int paren_depth = 0;
    while (i < text.size())
    {
        if (text[i] == '(') paren_depth++;
        else if (text[i] == ')')
        {
            if (paren_depth == 0) break;
            paren_depth--;
        }
        else if (text[i] == ' ' || text[i] == '"' || text[i] == '\'') break;
        i++;
    }
    url = text.substr(url_start, i - url_start);

    // Skip whitespace
    while (i < text.size() && text[i] == ' ') i++;

    // Optional title
    title.clear();
    if (i < text.size() && (text[i] == '"' || text[i] == '\''))
    {
        char quote = text[i];
        i++;
        size_t title_start = i;
        while (i < text.size() && text[i] != quote) i++;
        if (i < text.size())
        {
            title = text.substr(title_start, i - title_start);
            i++; // skip closing quote
        }
    }

    // Skip whitespace
    while (i < text.size() && text[i] == ' ') i++;

    if (i < text.size() && text[i] == ')')
    {
        end = i + 1;
        return true;
    }
    return false;
}

static std::string process_inline(const std::string& text, const RefMap& refs)
{
    std::string out;
    out.reserve(text.size() + text.size() / 2);
    size_t i = 0;
    size_t len = text.size();

    while (i < len)
    {
        char c = text[i];

        // --- Backslash escape ---
        if (c == '\\' && i + 1 < len && is_punctuation(text[i + 1]))
        {
            out += escape_html(std::string(1, text[i + 1]));
            i += 2;
            continue;
        }

        // --- Code span ---
        if (c == '`')
        {
            size_t bt = 0;
            while (i + bt < len && text[i + bt] == '`') bt++;
            auto closing = text.find(std::string(bt, '`'), i + bt);
            if (closing != std::string::npos)
            {
                auto code = text.substr(i + bt, closing - i - bt);
                // Strip one leading and one trailing space if both present
                if (code.size() >= 2 && code.front() == ' ' && code.back() == ' ')
                    code = code.substr(1, code.size() - 2);
                out += "<code>" + escape_html(code) + "</code>";
                i = closing + bt;
                continue;
            }
            // No closing backticks found — output literally
            out += std::string(bt, '`');
            i += bt;
            continue;
        }

        // --- HTML tag passthrough ---
        if (c == '<')
        {
            // Autolink: <url> or <email>
            if (i + 1 < len && (std::isalpha(text[i + 1]) || text[i + 1] == '/'))
            {
                auto close = text.find('>', i + 1);
                if (close != std::string::npos)
                {
                    auto inner = text.substr(i + 1, close - i - 1);
                    // Check if it looks like a URL (contains ://)
                    if (inner.find("://") != std::string::npos)
                    {
                        // Autolink
                        out += "<a href=\"" + escape_html(inner) + "\">" + escape_html(inner) + "</a>";
                        i = close + 1;
                        continue;
                    }
                    // Otherwise treat as HTML tag — pass through
                    out += text.substr(i, close - i + 1);
                    i = close + 1;
                    continue;
                }
            }
            out += "&lt;";
            i++;
            continue;
        }

        // --- Image: ![alt](url) or ![alt][ref] ---
        if (c == '!' && i + 1 < len && text[i + 1] == '[')
        {
            auto close_bracket = text.find(']', i + 2);
            if (close_bracket != std::string::npos)
            {
                auto alt = text.substr(i + 2, close_bracket - i - 2);

                // Inline image: ![alt](url "title")
                if (close_bracket + 1 < len && text[close_bracket + 1] == '(')
                {
                    std::string url, title;
                    size_t end;
                    if (parse_link_dest(text, close_bracket + 1, end, url, title))
                    {
                        out += "<img src=\"" + escape_html(url) + "\" alt=\"" + escape_html(alt) + "\"";
                        if (!title.empty())
                            out += " title=\"" + escape_html(title) + "\"";
                        out += " loading=\"lazy\">";
                        i = end;
                        continue;
                    }
                }

                // Reference image: ![alt][ref]
                if (close_bracket + 1 < len && text[close_bracket + 1] == '[')
                {
                    auto close_ref = text.find(']', close_bracket + 2);
                    if (close_ref != std::string::npos)
                    {
                        auto ref_key = to_lower(trim(text.substr(close_bracket + 2, close_ref - close_bracket - 2)));
                        if (ref_key.empty()) ref_key = to_lower(alt);
                        auto it = refs.find(ref_key);
                        if (it != refs.end())
                        {
                            out += "<img src=\"" + escape_html(it->second.url) + "\" alt=\"" + escape_html(alt) + "\"";
                            if (!it->second.title.empty())
                                out += " title=\"" + escape_html(it->second.title) + "\"";
                            out += " loading=\"lazy\">";
                            i = close_ref + 1;
                            continue;
                        }
                    }
                }
            }
            out += "!";
            i++;
            continue;
        }

        // --- Footnote reference: [^id] → sidenote ---
        if (c == '[' && i + 1 < len && text[i + 1] == '^')
        {
            auto close = text.find(']', i + 2);
            if (close != std::string::npos)
            {
                auto id = text.substr(i + 2, close - i - 2);
                out += "<label class=\"sidenote-toggle\" for=\"sn-" + escape_html(id) + "\">"
                       "<sup>" + escape_html(id) + "</sup></label>"
                       "<input type=\"checkbox\" class=\"sidenote-checkbox\" id=\"sn-" + escape_html(id) + "\">"
                       "<span class=\"sidenote\" id=\"fn-" + escape_html(id) + "\">";
                // Inline the footnote content if available — will be filled by post-processing
                out += "</span>";
                i = close + 1;
                continue;
            }
        }

        // --- Link: [text](url) or [text][ref] or [text] ---
        if (c == '[')
        {
            // Find the closing ]
            // Need to handle nested brackets for things like [![img](url)](link)
            int bracket_depth = 0;
            size_t close_bracket = std::string::npos;
            for (size_t j = i + 1; j < len; j++)
            {
                if (text[j] == '\\' && j + 1 < len) { j++; continue; }
                if (text[j] == '[') bracket_depth++;
                if (text[j] == ']')
                {
                    if (bracket_depth == 0) { close_bracket = j; break; }
                    bracket_depth--;
                }
            }

            if (close_bracket != std::string::npos)
            {
                auto link_text = text.substr(i + 1, close_bracket - i - 1);

                // Inline link: [text](url "title") or [text](url)^
                if (close_bracket + 1 < len && text[close_bracket + 1] == '(')
                {
                    std::string url, title;
                    size_t end;
                    if (parse_link_dest(text, close_bracket + 1, end, url, title))
                    {
                        bool new_tab = (end < len && text[end] == '^');
                        if (new_tab) ++end;
                        out += "<a href=\"" + escape_html(url) + "\"";
                        if (!title.empty())
                            out += " title=\"" + escape_html(title) + "\"";
                        if (new_tab)
                            out += " target=\"_blank\" rel=\"noopener noreferrer\"";
                        out += ">" + process_inline(link_text, refs) + "</a>";
                        i = end;
                        continue;
                    }
                }

                // Reference link: [text][ref] or [text][ref]^
                if (close_bracket + 1 < len && text[close_bracket + 1] == '[')
                {
                    auto close_ref = text.find(']', close_bracket + 2);
                    if (close_ref != std::string::npos)
                    {
                        auto ref_key = to_lower(trim(text.substr(close_bracket + 2, close_ref - close_bracket - 2)));
                        if (ref_key.empty()) ref_key = to_lower(link_text);
                        auto it = refs.find(ref_key);
                        if (it != refs.end())
                        {
                            bool new_tab = (close_ref + 1 < len && text[close_ref + 1] == '^');
                            out += "<a href=\"" + escape_html(it->second.url) + "\"";
                            if (!it->second.title.empty())
                                out += " title=\"" + escape_html(it->second.title) + "\"";
                            if (new_tab)
                                out += " target=\"_blank\" rel=\"noopener noreferrer\"";
                            out += ">" + process_inline(link_text, refs) + "</a>";
                            i = close_ref + 1 + (new_tab ? 1 : 0);
                            continue;
                        }
                    }
                }

                // Shortcut reference: [text] or [text]^
                {
                    auto ref_key = to_lower(trim(link_text));
                    auto it = refs.find(ref_key);
                    if (it != refs.end())
                    {
                        bool new_tab = (close_bracket + 1 < len && text[close_bracket + 1] == '^');
                        out += "<a href=\"" + escape_html(it->second.url) + "\"";
                        if (!it->second.title.empty())
                            out += " title=\"" + escape_html(it->second.title) + "\"";
                        if (new_tab)
                            out += " target=\"_blank\" rel=\"noopener noreferrer\"";
                        out += ">" + process_inline(link_text, refs) + "</a>";
                        i = close_bracket + 1 + (new_tab ? 1 : 0);
                        continue;
                    }
                }
            }
            // Not a link — output the bracket
            out += "[";
            i++;
            continue;
        }

        // --- Strikethrough: ~~text~~ ---
        if (c == '~' && i + 1 < len && text[i + 1] == '~')
        {
            auto end = text.find("~~", i + 2);
            if (end != std::string::npos)
            {
                out += "<del>" + process_inline(text.substr(i + 2, end - i - 2), refs) + "</del>";
                i = end + 2;
                continue;
            }
        }

        // --- Bold: ** or __ ---
        if ((c == '*' && i + 1 < len && text[i + 1] == '*') ||
            (c == '_' && i + 1 < len && text[i + 1] == '_'))
        {
            char d = c;
            // Check for ***bold italic*** or ___bold italic___
            if (i + 2 < len && text[i + 2] == d)
            {
                auto end = text.find(std::string(3, d), i + 3);
                if (end != std::string::npos)
                {
                    out += "<strong><em>" + process_inline(text.substr(i + 3, end - i - 3), refs) + "</em></strong>";
                    i = end + 3;
                    continue;
                }
            }
            auto end = text.find(std::string(2, d), i + 2);
            if (end != std::string::npos)
            {
                out += "<strong>" + process_inline(text.substr(i + 2, end - i - 2), refs) + "</strong>";
                i = end + 2;
                continue;
            }
        }

        // --- Italic: * or _ ---
        if (c == '*' || c == '_')
        {
            char d = c;
            // Find matching single closing delimiter
            size_t end = std::string::npos;
            for (size_t j = i + 1; j < len; j++)
            {
                if (text[j] == '\\' && j + 1 < len) { j++; continue; }
                if (text[j] == d)
                {
                    // For *, any position works
                    // For _, must not be inside a word (simplified: check boundaries)
                    if (d == '*' || j + 1 >= len || !std::isalnum(text[j + 1]))
                    {
                        // Make sure it's not ** (double)
                        if (j + 1 >= len || text[j + 1] != d)
                        {
                            end = j;
                            break;
                        }
                    }
                }
            }
            if (end != std::string::npos && end > i + 1)
            {
                out += "<em>" + process_inline(text.substr(i + 1, end - i - 1), refs) + "</em>";
                i = end + 1;
                continue;
            }
            // Not emphasis — output literally
            out += c;
            i++;
            continue;
        }

        // --- Line break: two trailing spaces before newline ---
        if (c == ' ' && i + 1 < len)
        {
            size_t spaces = 0;
            while (i + spaces < len && text[i + spaces] == ' ') spaces++;
            if (spaces >= 2 && i + spaces < len && text[i + spaces] == '\n')
            {
                out += "<br>\n";
                i += spaces + 1;
                continue;
            }
        }

        // --- Newline (soft break) ---
        if (c == '\n')
        {
            out += "\n";
            i++;
            continue;
        }

        // --- Smart typography ---
        // Em dash: ---
        if (c == '-' && i + 2 < len && text[i + 1] == '-' && text[i + 2] == '-')
        {
            out += "&mdash;";
            i += 3;
            continue;
        }
        // En dash: --
        if (c == '-' && i + 1 < len && text[i + 1] == '-')
        {
            out += "&ndash;";
            i += 2;
            continue;
        }
        // Ellipsis: ...
        if (c == '.' && i + 2 < len && text[i + 1] == '.' && text[i + 2] == '.')
        {
            out += "&hellip;";
            i += 3;
            continue;
        }
        // Smart double quotes
        if (c == '"')
        {
            // Opening quote: at start, after space/newline, or after (
            bool is_open = (i == 0) ||
                           (i > 0 && (text[i - 1] == ' ' || text[i - 1] == '\n' ||
                                      text[i - 1] == '\t' || text[i - 1] == '('));
            out += is_open ? "&ldquo;" : "&rdquo;";
            i++;
            continue;
        }
        // Smart single quotes / apostrophes
        if (c == '\'')
        {
            bool is_open = (i == 0) ||
                           (i > 0 && (text[i - 1] == ' ' || text[i - 1] == '\n' ||
                                      text[i - 1] == '\t' || text[i - 1] == '('));
            out += is_open ? "&lsquo;" : "&rsquo;";
            i++;
            continue;
        }

        // --- Regular character ---
        if (c == '&') out += "&amp;";
        else if (c == '>') out += "&gt;";
        else out += c;
        i++;
    }

    return out;
}

// ============================================================================
// Block-level processing
// ============================================================================

// Forward declaration
static std::string process_blocks(const std::vector<std::string>& lines,
                                  const RefMap& refs,
                                  const std::map<std::string, FootnoteDef>& footnotes,
                                  std::unordered_map<std::string, int>& heading_slugs);

// --- Line classification helpers ---

static bool is_blank(const std::string& s)
{
    return s.find_first_not_of(" \t\r\n") == std::string::npos;
}

static bool is_hr(const std::string& s)
{
    auto t = trim(s);
    if (t.size() < 3) return false;
    char ch = t[0];
    if (ch != '-' && ch != '*' && ch != '_') return false;
    size_t count = 0;
    for (char c : t)
    {
        if (c == ch) count++;
        else if (c != ' ' && c != '\t') return false;
    }
    return count >= 3;
}

static int atx_heading_level(const std::string& s)
{
    auto t = ltrim(s);
    if (t.empty() || t[0] != '#') return 0;
    int level = 0;
    while (level < (int)t.size() && t[level] == '#') level++;
    if (level > 6) return 0;
    if (level < (int)t.size() && t[level] != ' ' && t[level] != '\t') return 0;
    return level;
}

static std::string atx_heading_text(const std::string& s, int level)
{
    auto t = ltrim(s);
    auto text = t.substr(level);
    if (!text.empty() && text[0] == ' ') text = text.substr(1);
    // Remove trailing # characters
    while (!text.empty() && text.back() == '#') text.pop_back();
    while (!text.empty() && text.back() == ' ') text.pop_back();
    return text;
}

static bool is_setext_underline(const std::string& s, int& level)
{
    auto t = trim(s);
    if (t.empty()) return false;
    if (t[0] == '=')
    {
        for (char c : t) if (c != '=') return false;
        level = 1;
        return true;
    }
    if (t[0] == '-')
    {
        for (char c : t) if (c != '-') return false;
        if (t.size() >= 2)
        {
            level = 2;
            return true;
        }
    }
    return false;
}

static bool is_fenced_code(const std::string& s, std::string& lang, char& fence_char, size_t& fence_len)
{
    auto t = ltrim(s);
    if (t.size() >= 3 && (t[0] == '`' || t[0] == '~'))
    {
        char ch = t[0];
        size_t count = 0;
        while (count < t.size() && t[count] == ch) count++;
        if (count >= 3)
        {
            fence_char = ch;
            fence_len = count;
            lang = trim(t.substr(count));
            return true;
        }
    }
    return false;
}

static bool is_fenced_code_end(const std::string& s, char fence_char, size_t fence_len)
{
    auto t = ltrim(s);
    if (t.size() >= fence_len)
    {
        size_t count = 0;
        while (count < t.size() && t[count] == fence_char) count++;
        if (count >= fence_len)
        {
            // Closing fence must be only fence chars + optional trailing whitespace
            auto rest = t.substr(count);
            return rest.find_first_not_of(" \t") == std::string::npos;
        }
    }
    return false;
}

static bool is_blockquote(const std::string& s)
{
    auto t = ltrim(s);
    return !t.empty() && t[0] == '>';
}

static std::string strip_blockquote(const std::string& s)
{
    auto t = ltrim(s);
    if (t.empty() || t[0] != '>') return s;
    t = t.substr(1);
    if (!t.empty() && t[0] == ' ') t = t.substr(1);
    return t;
}

// Detect unordered list item. Returns marker length (e.g., 2 for "- ") or 0
static size_t is_ul_item(const std::string& s)
{
    auto t = ltrim(s);
    if (t.size() >= 2 && (t[0] == '-' || t[0] == '*' || t[0] == '+') && t[1] == ' ')
    {
        // Make sure it's not a horizontal rule (---, ***, etc.)
        if (t[0] == '-' || t[0] == '*')
        {
            bool all_same = true;
            for (size_t i = 0; i < t.size(); i++)
            {
                if (t[i] != t[0] && t[i] != ' ') { all_same = false; break; }
            }
            size_t marker_count = 0;
            for (char c : t) if (c == t[0]) marker_count++;
            if (all_same && marker_count >= 3) return 0; // It's a horizontal rule
        }
        return leading_spaces(s) + 2;
    }
    return 0;
}

// Detect ordered list item. Returns content offset or 0
static size_t is_ol_item(const std::string& s)
{
    auto t = ltrim(s);
    size_t i = 0;
    while (i < t.size() && std::isdigit(t[i])) i++;
    if (i == 0 || i > 9) return 0;
    if (i < t.size() && (t[i] == '.' || t[i] == ')') && i + 1 < t.size() && t[i + 1] == ' ')
        return leading_spaces(s) + i + 2;
    return 0;
}

// Detect task list checkbox
static bool is_task_item(const std::string& text, bool& checked, std::string& rest)
{
    if (text.size() >= 3 && text[0] == '[' && text[2] == ']')
    {
        if (text[1] == 'x' || text[1] == 'X') { checked = true; }
        else if (text[1] == ' ') { checked = false; }
        else return false;
        rest = text.size() > 3 ? text.substr(4) : "";
        return true;
    }
    return false;
}

// Detect table separator line
static bool is_table_separator(const std::string& s)
{
    auto t = trim(s);
    if (t.empty()) return false;
    // Strip leading/trailing |
    if (t.front() == '|') t = t.substr(1);
    if (!t.empty() && t.back() == '|') t = t.substr(0, t.size() - 1);
    t = trim(t);
    if (t.empty()) return false;

    // Each cell must be :?-{3,}:? separated by |
    size_t dashes = 0;
    for (size_t i = 0; i <= t.size(); i++)
    {
        char c = (i < t.size()) ? t[i] : '|';
        if (c == '|' || i == t.size())
        {
            if (dashes < 3) return false;
            dashes = 0;
        }
        else if (c == '-') { dashes++; }
        else if (c == ':') { }
        else if (c == ' ' || c == '\t') { /* skip */ }
        else return false;
    }
    return true;
}

// Parse table row into cells
static std::vector<std::string> parse_table_row(const std::string& s)
{
    std::vector<std::string> cells;
    auto t = trim(s);
    if (!t.empty() && t.front() == '|') t = t.substr(1);
    if (!t.empty() && t.back() == '|') t = t.substr(0, t.size() - 1);

    std::string cell;
    for (size_t i = 0; i < t.size(); i++)
    {
        if (t[i] == '\\' && i + 1 < t.size() && t[i + 1] == '|')
        {
            cell += '|';
            i++;
        }
        else if (t[i] == '|')
        {
            cells.push_back(trim(cell));
            cell.clear();
        }
        else
        {
            cell += t[i];
        }
    }
    cells.push_back(trim(cell));
    return cells;
}

// Parse table alignment from separator line
static std::vector<std::string> parse_table_align(const std::string& s)
{
    auto cells = parse_table_row(s);
    std::vector<std::string> aligns;
    for (auto& c : cells)
    {
        auto t = trim(c);
        bool left = !t.empty() && t.front() == ':';
        bool right = !t.empty() && t.back() == ':';
        if (left && right) aligns.push_back("center");
        else if (right) aligns.push_back("right");
        else if (left) aligns.push_back("left");
        else aligns.push_back("");
    }
    return aligns;
}

// Detect if a line looks like a reference definition: [key]: url "title"
static bool parse_ref_def(const std::string& s, std::string& key, RefDef& def)
{
    auto t = ltrim(s);
    if (t.empty() || t[0] != '[') return false;
    auto close = t.find("]:");
    if (close == std::string::npos) return false;
    key = to_lower(trim(t.substr(1, close - 1)));
    auto rest = trim(t.substr(close + 2));

    // Parse URL (possibly in angle brackets)
    std::string url;
    size_t i = 0;
    if (!rest.empty() && rest[0] == '<')
    {
        auto end = rest.find('>');
        if (end != std::string::npos)
        {
            url = rest.substr(1, end - 1);
            i = end + 1;
        }
        else return false;
    }
    else
    {
        while (i < rest.size() && rest[i] != ' ' && rest[i] != '\t')
        {
            url += rest[i];
            i++;
        }
    }

    def.url = url;
    def.title.clear();

    // Optional title
    while (i < rest.size() && (rest[i] == ' ' || rest[i] == '\t')) i++;
    if (i < rest.size() && (rest[i] == '"' || rest[i] == '\'' || rest[i] == '('))
    {
        char open = rest[i];
        char close_ch = (open == '(') ? ')' : open;
        i++;
        size_t title_start = i;
        while (i < rest.size() && rest[i] != close_ch) i++;
        def.title = rest.substr(title_start, i - title_start);
    }

    return true;
}

// Detect footnote definition: [^id]: content
static bool parse_footnote_def(const std::string& s, std::string& id, std::string& content)
{
    auto t = ltrim(s);
    if (t.size() < 5 || t[0] != '[' || t[1] != '^') return false;
    auto close = t.find("]:");
    if (close == std::string::npos || close <= 2) return false;
    id = t.substr(2, close - 2);
    content = trim(t.substr(close + 2));
    return true;
}

// Detect HTML block start
static bool is_html_block_start(const std::string& s)
{
    auto t = ltrim(s);
    if (t.size() < 2 || t[0] != '<') return false;

    static const char* tags[] = {
        "div", "pre", "p", "table", "tr", "td", "th", "thead", "tbody", "tfoot",
        "ul", "ol", "li", "dl", "dt", "dd", "h1", "h2", "h3", "h4", "h5", "h6",
        "blockquote", "hr", "form", "fieldset", "iframe", "script", "style",
        "section", "article", "nav", "aside", "header", "footer", "main",
        "figure", "figcaption", "details", "summary", "address", "canvas",
        nullptr
    };

    // Also match <!-- comments
    if (t.size() >= 4 && t.substr(0, 4) == "<!--") return true;

    size_t tag_start = 1;
    if (t[1] == '/') tag_start = 2;

    for (int j = 0; tags[j]; j++)
    {
        std::string tag = tags[j];
        if (t.size() > tag_start + tag.size())
        {
            auto candidate = to_lower(t.substr(tag_start, tag.size()));
            if (candidate == tag)
            {
                char next = t[tag_start + tag.size()];
                if (next == ' ' || next == '>' || next == '/' || next == '\t' || next == '\n')
                    return true;
            }
        }
        // Self-closing or exact
        else if (t.size() == tag_start + tag.size() + 1)
        {
            auto candidate = to_lower(t.substr(tag_start, tag.size()));
            if (candidate == tag && t.back() == '>')
                return true;
        }
    }

    // Inline HTML elements with attributes on their own line (like <a href=...>)
    if (t[1] != '/' && std::isalpha(t[1]))
    {
        // Check if the line starts and potentially contains a full block-level tag
        size_t end = 1;
        while (end < t.size() && std::isalnum(t[end])) end++;
        // If the rest of the line has attributes and closes with >, it's an HTML block
        if (end < t.size() && (t[end] == ' ' || t[end] == '>'))
        {
            auto tagname = to_lower(t.substr(1, end - 1));
            // Only treat known block/inline-block tags as HTML blocks
            for (int j = 0; tags[j]; j++)
            {
                if (tagname == tags[j]) return true;
            }
            // Also pass through a, img if they appear as standalone lines
            if (tagname == "a" || tagname == "img" || tagname == "br" || tagname == "input")
                return true;
        }
    }

    return false;
}

// ============================================================================
// List processing
// ============================================================================

struct ListEntry
{
    size_t indent;
    bool ordered;
    std::string content; // First line content (after marker)
    std::vector<std::string> continuation; // Subsequent lines
};

static std::string render_list_entries(const std::vector<ListEntry>& entries,
                                       size_t start, size_t end,
                                       size_t base_indent,
                                       const RefMap& refs,
                                       const std::map<std::string, FootnoteDef>& footnotes);

static std::string render_list_entries(const std::vector<ListEntry>& entries,
                                       size_t start, size_t end_idx,
                                       size_t base_indent,
                                       const RefMap& refs,
                                       const std::map<std::string, FootnoteDef>& footnotes)
{
    if (start >= end_idx) return "";

    bool ordered = entries[start].ordered;
    std::string tag = ordered ? "ol" : "ul";
    std::string html = "<" + tag + ">\n";

    size_t i = start;
    while (i < end_idx)
    {
        if (entries[i].indent > base_indent)
        {
            // Find the range of this sub-list
            size_t sub_start = i;
            size_t sub_indent = entries[i].indent;
            while (i < end_idx && entries[i].indent >= sub_indent) i++;
            // Render sub-list (append to previous li if possible)
            // We need to remove the closing </li> from previous item and re-add after sublist
            // Simpler: just output nested list
            html += render_list_entries(entries, sub_start, i, sub_indent, refs, footnotes);
            continue;
        }

        auto& entry = entries[i];
        std::string item_content = entry.content;

        // Add continuation lines
        for (auto& cont : entry.continuation)
            item_content += " " + trim(cont);

        // Check for task list
        bool checked = false;
        std::string task_rest;
        if (is_task_item(item_content, checked, task_rest))
        {
            std::string checkbox = checked
                ? "<input type=\"checkbox\" checked disabled> "
                : "<input type=\"checkbox\" disabled> ";
            html += "<li>" + checkbox + process_inline(task_rest, refs);
        }
        else
        {
            html += "<li>" + process_inline(item_content, refs);
        }

        // Check if next entries are sub-items
        size_t next = i + 1;
        if (next < end_idx && entries[next].indent > base_indent)
        {
            size_t sub_start = next;
            size_t sub_indent = entries[next].indent;
            while (next < end_idx && entries[next].indent >= sub_indent) next++;
            html += "\n" + render_list_entries(entries, sub_start, next, sub_indent, refs, footnotes);
            i = next;
        }
        else
        {
            i++;
        }

        html += "</li>\n";
    }

    html += "</" + tag + ">\n";
    return html;
}

// ============================================================================
// Main block processor
// ============================================================================

static std::string process_blocks(const std::vector<std::string>& lines,
                                  const RefMap& refs,
                                  const std::map<std::string, FootnoteDef>& footnotes,
                                  std::unordered_map<std::string, int>& heading_slugs)
{
    std::string html;
    html.reserve(lines.size() * 80);
    size_t i = 0;
    size_t n = lines.size();

    std::vector<std::string> paragraph;

    auto flush_paragraph = [&]() {
        if (paragraph.empty()) return;
        std::string text;
        for (size_t j = 0; j < paragraph.size(); j++)
        {
            if (j > 0) text += "\n";
            text += paragraph[j];
        }
        html += "<p>" + process_inline(text, refs) + "</p>\n";
        paragraph.clear();
    };

    while (i < n)
    {
        const auto& line = lines[i];

        // --- Blank line ---
        if (is_blank(line))
        {
            flush_paragraph();
            i++;
            continue;
        }

        // --- Fenced code block ---
        {
            std::string lang;
            char fence_char;
            size_t fence_len;
            if (is_fenced_code(line, lang, fence_char, fence_len))
            {
                flush_paragraph();
                i++;
                std::string raw;
                while (i < n && !is_fenced_code_end(lines[i], fence_char, fence_len))
                {
                    raw += lines[i];
                    raw += '\n';
                    i++;
                }
                if (i < n) i++; // skip closing fence

                // Parse optional title="filename" from info string
                std::string title;
                std::string lang_only = lang;
                {
                    auto tpos = lang.find("title=");
                    if (tpos != std::string::npos)
                    {
                        lang_only = trim(lang.substr(0, tpos));
                        auto val_start = tpos + 6;
                        if (val_start < lang.size() && (lang[val_start] == '"' || lang[val_start] == '\''))
                        {
                            char q = lang[val_start];
                            auto val_end = lang.find(q, val_start + 1);
                            if (val_end != std::string::npos)
                                title = lang.substr(val_start + 1, val_end - val_start - 1);
                        }
                        else
                        {
                            title = trim(lang.substr(val_start));
                        }
                    }
                }

                if (!title.empty())
                    html += "<div class=\"code-block\"><div class=\"code-title\">" + escape_html(title) + "</div>";
                else
                    html += "<div class=\"code-block\">";

                if (!lang_only.empty())
                    html += "<pre><code class=\"language-" + escape_html(lang_only) + "\">";
                else
                    html += "<pre><code>";
                html += escape_html(raw) + "</code></pre></div>\n";
                continue;
            }
        }

        // --- Reference definition (skip, already collected) ---
        {
            std::string key;
            RefDef def;
            if (parse_ref_def(line, key, def))
            {
                flush_paragraph();
                i++;
                continue;
            }
        }

        // --- Footnote definition (skip, already collected) ---
        {
            std::string fn_id, fn_content;
            if (parse_footnote_def(line, fn_id, fn_content))
            {
                flush_paragraph();
                i++;
                // Skip continuation lines (indented)
                while (i < n && !is_blank(lines[i]) && leading_spaces(lines[i]) >= 4)
                    i++;
                continue;
            }
        }

        // --- ATX Heading ---
        {
            int level = atx_heading_level(line);
            if (level > 0)
            {
                flush_paragraph();
                auto text = atx_heading_text(line, level);
                auto id = make_heading_id(text, heading_slugs);
                auto lvl = std::to_string(level);
                html += "<h" + lvl + " id=\"" + id + "\">"
                      + "<a class=\"heading-anchor\" href=\"#" + id + "\" aria-hidden=\"true\">#</a>"
                      + process_inline(text, refs)
                      + "</h" + lvl + ">\n";
                i++;
                continue;
            }
        }

        // --- Setext heading (check if next line is === or ---) ---
        if (i + 1 < n && !is_blank(line))
        {
            int slevel = 0;
            if (is_setext_underline(lines[i + 1], slevel) && leading_spaces(line) < 4)
            {
                // But only if the current line isn't already something else
                // (not a list item, not a blockquote, etc.)
                if (!is_ul_item(line) && !is_ol_item(line) && !is_blockquote(line))
                {
                    flush_paragraph();
                    auto text = trim(line);
                    auto id = make_heading_id(text, heading_slugs);
                    auto lvl = std::to_string(slevel);
                    html += "<h" + lvl + " id=\"" + id + "\">"
                          + "<a class=\"heading-anchor\" href=\"#" + id + "\" aria-hidden=\"true\">#</a>"
                          + process_inline(text, refs)
                          + "</h" + lvl + ">\n";
                    i += 2;
                    continue;
                }
            }
        }

        // --- Horizontal rule ---
        if (is_hr(line))
        {
            flush_paragraph();
            html += "<hr>\n";
            i++;
            continue;
        }

        // --- HTML block ---
        if (is_html_block_start(line) && paragraph.empty())
        {
            flush_paragraph();
            // Pass through lines until blank line or end
            while (i < n && !is_blank(lines[i]))
            {
                html += lines[i] + "\n";
                i++;
            }
            continue;
        }

        // --- Blockquote ---
        if (is_blockquote(line))
        {
            flush_paragraph();
            std::vector<std::string> bq_lines;
            while (i < n)
            {
                if (is_blockquote(lines[i]))
                {
                    bq_lines.push_back(strip_blockquote(lines[i]));
                    i++;
                }
                else if (!is_blank(lines[i]) && !bq_lines.empty() && !is_blank(bq_lines.back()))
                {
                    // Lazy continuation
                    bq_lines.push_back(lines[i]);
                    i++;
                }
                else
                {
                    break;
                }
            }
            html += "<blockquote>\n" + process_blocks(bq_lines, refs, footnotes, heading_slugs) + "</blockquote>\n";
            continue;
        }

        // --- Table ---
        if (i + 1 < n && line.find('|') != std::string::npos && is_table_separator(lines[i + 1]))
        {
            flush_paragraph();
            auto headers = parse_table_row(line);
            auto aligns = parse_table_align(lines[i + 1]);
            i += 2;

            html += "<table>\n<thead>\n<tr>\n";
            for (size_t c = 0; c < headers.size(); c++)
            {
                std::string style;
                if (c < aligns.size() && !aligns[c].empty())
                    style = " style=\"text-align: " + aligns[c] + "\"";
                html += "<th" + style + ">" + process_inline(headers[c], refs) + "</th>\n";
            }
            html += "</tr>\n</thead>\n<tbody>\n";

            while (i < n && !is_blank(lines[i]) && lines[i].find('|') != std::string::npos)
            {
                auto cells = parse_table_row(lines[i]);
                html += "<tr>\n";
                for (size_t c = 0; c < headers.size(); c++)
                {
                    std::string style;
                    if (c < aligns.size() && !aligns[c].empty())
                        style = " style=\"text-align: " + aligns[c] + "\"";
                    std::string cell_content = (c < cells.size()) ? cells[c] : "";
                    html += "<td" + style + ">" + process_inline(cell_content, refs) + "</td>\n";
                }
                html += "</tr>\n";
                i++;
            }
            html += "</tbody>\n</table>\n";
            continue;
        }

        // --- List (ordered or unordered) ---
        {
            size_t ul = is_ul_item(line);
            size_t ol = is_ol_item(line);
            if (ul || ol)
            {
                flush_paragraph();
                std::vector<ListEntry> entries;

                while (i < n)
                {
                    size_t ul_off = is_ul_item(lines[i]);
                    size_t ol_off = is_ol_item(lines[i]);

                    if (ul_off)
                    {
                        ListEntry entry;
                        entry.indent = leading_spaces(lines[i]);
                        entry.ordered = false;
                        entry.content = lines[i].substr(entry.indent + 2);
                        entries.push_back(entry);
                        i++;
                    }
                    else if (ol_off)
                    {
                        ListEntry entry;
                        entry.indent = leading_spaces(lines[i]);
                        entry.ordered = true;
                        entry.content = strip_leading(lines[i], ol_off);
                        // Actually strip indent + marker
                        auto t = ltrim(lines[i]);
                        size_t marker_end = 0;
                        while (marker_end < t.size() && std::isdigit(t[marker_end])) marker_end++;
                        marker_end += 2; // skip ". "
                        entry.content = t.substr(marker_end);
                        entries.push_back(entry);
                        i++;
                    }
                    else if (!is_blank(lines[i]) && leading_spaces(lines[i]) >= 2 && !entries.empty())
                    {
                        // Continuation line for the current list item
                        entries.back().continuation.push_back(lines[i]);
                        i++;
                    }
                    else if (is_blank(lines[i]))
                    {
                        // Blank line — could be between list items
                        if (i + 1 < n && (is_ul_item(lines[i + 1]) || is_ol_item(lines[i + 1]) ||
                                          leading_spaces(lines[i + 1]) >= 2))
                        {
                            i++; // Skip blank line within list
                        }
                        else
                        {
                            break; // End of list
                        }
                    }
                    else
                    {
                        break;
                    }
                }

                if (!entries.empty())
                {
                    html += render_list_entries(entries, 0, entries.size(),
                                               entries[0].indent, refs, footnotes);
                }
                continue;
            }
        }

        // --- Indented code block (4+ spaces, not in paragraph) ---
        if (leading_spaces(line) >= 4 && paragraph.empty())
        {
            std::string code;
            while (i < n && (leading_spaces(lines[i]) >= 4 || is_blank(lines[i])))
            {
                if (is_blank(lines[i]))
                    code += "\n";
                else
                    code += escape_html(strip_leading(lines[i], 4)) + "\n";
                i++;
            }
            // Remove trailing blank lines
            while (!code.empty() && code.back() == '\n')
            {
                auto pos = code.find_last_not_of('\n');
                if (pos != std::string::npos)
                {
                    code = code.substr(0, pos + 1) + "\n";
                    break;
                }
                else
                {
                    code.clear();
                }
            }
            html += "<pre><code>" + code + "</code></pre>\n";
            continue;
        }

        // --- Paragraph text ---
        paragraph.push_back(line);
        i++;
    }

    flush_paragraph();
    return html;
}

// ============================================================================
// Public API
// ============================================================================

std::string markdown_to_html(const std::string& markdown)
{
    auto lines = split_lines(markdown);

    // First pass: collect reference definitions and footnotes
    RefMap refs;
    std::map<std::string, FootnoteDef> footnotes;

    for (size_t i = 0; i < lines.size(); i++)
    {
        // Skip fenced code blocks in pass 1
        std::string skip_lang;
        char skip_fc;
        size_t skip_fl;
        if (is_fenced_code(lines[i], skip_lang, skip_fc, skip_fl))
        {
            i++;
            while (i < lines.size() && !is_fenced_code_end(lines[i], skip_fc, skip_fl))
                i++;
            continue; // skip closing fence
        }

        std::string key;
        RefDef def;
        if (parse_ref_def(lines[i], key, def))
        {
            // Make sure it's not also a footnote
            if (key.empty() || key[0] != '^')
                refs[key] = def;
        }

        std::string fn_id, fn_content;
        if (parse_footnote_def(lines[i], fn_id, fn_content))
        {
            FootnoteDef fndef;
            fndef.id = fn_id;
            fndef.content = fn_content;
            // Collect continuation lines
            size_t j = i + 1;
            while (j < lines.size() && !is_blank(lines[j]) && leading_spaces(lines[j]) >= 4)
            {
                fndef.content += " " + trim(lines[j]);
                j++;
            }
            footnotes[fn_id] = fndef;
        }
    }

    // Second pass: process blocks
    std::unordered_map<std::string, int> heading_slugs;
    std::string html = process_blocks(lines, refs, footnotes, heading_slugs);

    // Fill in sidenote content — replace empty sidenote spans with footnote content
    if (!footnotes.empty())
    {
        for (auto& [id, fndef] : footnotes)
        {
            auto marker = "id=\"fn-" + escape_html(id) + "\"></span>";
            auto pos = html.find(marker);
            if (pos != std::string::npos)
            {
                auto insert_pos = pos + marker.size() - 7; // before </span>
                auto content = process_inline(fndef.content, refs);
                html.insert(insert_pos, content);
            }
        }
    }

    return html;
}

}
