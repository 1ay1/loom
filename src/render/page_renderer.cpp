#include "../../include/loom/render/page_renderer.hpp"
#include "../../include/loom/render/dom.hpp"

namespace loom
{

using namespace dom;

std::string render_page(const Page& page)
{
    return article(class_("page"),
        h1(page.title.get()),
        div(class_("page-content"), raw(page.content.get()))
    ).render();
}

}
