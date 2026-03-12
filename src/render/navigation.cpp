#include "../../include/loom/render/render.hpp"

namespace loom
{

std::string render_navigation(const Navigation& nav)
{
    std::string html;

    html += "<nav><ul>";

    for(const auto& item : nav.items)
    {
        html += "<li>";
        html += "<a href=\"" + item.url + "\">";
        html += item.title;
        html += "</a>";
        html += "</li>";
    }

    html += "</ul></nav>";

    return html;
}

}
