#include "../../include/loom/render/themes.hpp"

#include "../../include/loom/render/theme/builtin/default.hpp"
#include "../../include/loom/render/theme/builtin/terminal.hpp"

namespace loom
{

namespace bt = theme::builtin;

static const std::map<std::string, theme::ThemeDef> THEMES = {
    {"default",  bt::default_theme},
    {"terminal", bt::terminal},
};

const std::map<std::string, theme::ThemeDef>& builtin_themes()
{
    return THEMES;
}

}
