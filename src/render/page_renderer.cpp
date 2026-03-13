#include "../../include/loom/render/page_renderer.hpp"

#include <string>

namespace loom
{

std::string render_page(const Page& page)
{
    std::string html;

    html += "<article class='page'>";

    html += "<h1>";
    html += page.title.get();
    html += "</h1>";

    html += "<div class='page-content'>";
    html += page.content.get();
    html += "</div>";

    html += "</article>";

    return html;
}

}
