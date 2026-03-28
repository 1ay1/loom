#include "../../include/loom/render/themes.hpp"

#include "../../include/loom/render/theme/builtin/default.hpp"
#include "../../include/loom/render/theme/builtin/terminal.hpp"
#include "../../include/loom/render/theme/builtin/nord.hpp"
#include "../../include/loom/render/theme/builtin/gruvbox.hpp"
#include "../../include/loom/render/theme/builtin/rose.hpp"

namespace loom
{

namespace bt = theme::builtin;

static const std::map<std::string, theme::ThemeDef> THEMES = {
    {"default",  bt::default_theme},
    {"terminal", bt::terminal},
    {"nord",     bt::nord},
    {"gruvbox",  bt::gruvbox},
    {"rose",     bt::rose},
};

const std::map<std::string, theme::ThemeDef>& builtin_themes()
{
    return THEMES;
}

}
