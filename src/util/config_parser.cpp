#include "../../include/loom/util/config_parser.hpp"

#include <sstream>

namespace loom
{

std::map<std::string, std::string> parse_config(const std::string& text)
{
    std::map<std::string, std::string> result;
    std::istringstream stream(text);
    std::string line;

    while (std::getline(stream, line))
    {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#')
            continue;

        auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;

        auto key = line.substr(0, eq);
        auto val = line.substr(eq + 1);

        // Trim whitespace
        auto trim = [](std::string& s) {
            auto start = s.find_first_not_of(" \t\r\n");
            auto end = s.find_last_not_of(" \t\r\n");
            s = (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
        };

        trim(key);
        trim(val);

        if (!key.empty())
            result[key] = val;
    }

    return result;
}

ParsedDocument parse_frontmatter(const std::string& text)
{
    ParsedDocument doc;

    // Check for frontmatter delimiter
    if (text.size() < 3 || text.substr(0, 3) != "---")
    {
        doc.body = text;
        return doc;
    }

    // Find closing ---
    auto end = text.find("\n---", 3);
    if (end == std::string::npos)
    {
        doc.body = text;
        return doc;
    }

    // Parse frontmatter as key: value
    std::string front = text.substr(4, end - 4); // skip "---\n"
    std::istringstream stream(front);
    std::string line;

    while (std::getline(stream, line))
    {
        if (line.empty() || line[0] == '#')
            continue;

        auto colon = line.find(':');
        if (colon == std::string::npos)
            continue;

        auto key = line.substr(0, colon);
        auto val = line.substr(colon + 1);

        auto trim = [](std::string& s) {
            auto start = s.find_first_not_of(" \t\r\n");
            auto end = s.find_last_not_of(" \t\r\n");
            s = (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
        };

        trim(key);
        trim(val);

        // Strip surrounding quotes (YAML-style)
        if (val.size() >= 2 &&
            ((val.front() == '"' && val.back() == '"') ||
             (val.front() == '\'' && val.back() == '\'')))
            val = val.substr(1, val.size() - 2);

        if (!key.empty())
            doc.meta[key] = val;
    }

    // Body is everything after closing ---
    auto body_start = end + 4; // skip "\n---"
    if (body_start < text.size() && text[body_start] == '\n')
        body_start++;

    doc.body = text.substr(body_start);

    return doc;
}

}
