#pragma once

#include "../domain/site.hpp"

#include <string>

namespace loom
{

std::string render_layout(
    const Site& site,
    const std::string& navigation,
    const std::string& content
);

}
