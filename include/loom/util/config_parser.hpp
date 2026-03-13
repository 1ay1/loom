#pragma once

#include <string>
#include <map>

namespace loom
{
    // Parses key = value config files. Lines starting with # are comments.
    std::map<std::string, std::string> parse_config(const std::string& text);

    // Parses --- delimited frontmatter from markdown files.
    // Returns {frontmatter_map, body_content}.
    struct ParsedDocument
    {
        std::map<std::string, std::string> meta;
        std::string body;
    };

    ParsedDocument parse_frontmatter(const std::string& text);
}
