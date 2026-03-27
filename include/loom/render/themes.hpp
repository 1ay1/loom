#pragma once

#include <map>
#include <string>
#include "theme/theme_def.hpp"
#include "theme/compiler.hpp"

namespace loom
{

const std::map<std::string, theme::ThemeDef>& builtin_themes();

}
