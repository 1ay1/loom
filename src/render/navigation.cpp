#include "../../include/loom/render/navigation.hpp"
#include "../../include/loom/render/dom.hpp"

namespace loom
{

using namespace dom;

std::string render_navigation(const Navigation& navigation)
{
    return nav(
        ul(each(navigation.items, [](const auto& item) {
            return li(a(href(item.url), item.title));
        }))
    ).render();
}

}
