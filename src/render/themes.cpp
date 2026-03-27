#include "../../include/loom/render/themes.hpp"

// Builtin theme definitions
#include "../../include/loom/render/theme/builtin/default.hpp"
#include "../../include/loom/render/theme/builtin/serif.hpp"
#include "../../include/loom/render/theme/builtin/mono.hpp"
#include "../../include/loom/render/theme/builtin/nord.hpp"
#include "../../include/loom/render/theme/builtin/rose.hpp"
#include "../../include/loom/render/theme/builtin/cobalt.hpp"
#include "../../include/loom/render/theme/builtin/earth.hpp"
#include "../../include/loom/render/theme/builtin/hacker.hpp"
#include "../../include/loom/render/theme/builtin/solarized.hpp"
#include "../../include/loom/render/theme/builtin/dracula.hpp"
#include "../../include/loom/render/theme/builtin/gruvbox.hpp"
#include "../../include/loom/render/theme/builtin/catppuccin.hpp"
#include "../../include/loom/render/theme/builtin/tokyonight.hpp"
#include "../../include/loom/render/theme/builtin/kanagawa.hpp"
#include "../../include/loom/render/theme/builtin/typewriter.hpp"
#include "../../include/loom/render/theme/builtin/brutalist.hpp"
#include "../../include/loom/render/theme/builtin/lavender.hpp"
#include "../../include/loom/render/theme/builtin/warm.hpp"
#include "../../include/loom/render/theme/builtin/ocean.hpp"
#include "../../include/loom/render/theme/builtin/sakura.hpp"
#include "../../include/loom/render/theme/builtin/midnight.hpp"
#include "../../include/loom/render/theme/builtin/terminal.hpp"

namespace loom
{

namespace bt = theme::builtin;

static const std::map<std::string, theme::ThemeDef> THEMES = {
    {"default",     bt::default_theme},
    {"serif",       bt::serif},
    {"mono",        bt::mono},
    {"nord",        bt::nord},
    {"rose",        bt::rose},
    {"cobalt",      bt::cobalt},
    {"earth",       bt::earth},
    {"hacker",      bt::hacker},
    {"solarized",   bt::solarized},
    {"dracula",     bt::dracula},
    {"gruvbox",     bt::gruvbox},
    {"catppuccin",  bt::catppuccin},
    {"tokyonight",  bt::tokyonight},
    {"kanagawa",    bt::kanagawa},
    {"typewriter",  bt::typewriter},
    {"brutalist",   bt::brutalist},
    {"lavender",    bt::lavender},
    {"warm",        bt::warm},
    {"ocean",       bt::ocean},
    {"sakura",      bt::sakura},
    {"midnight",    bt::midnight},
    {"terminal",    bt::terminal},
};

const std::map<std::string, theme::ThemeDef>& builtin_themes()
{
    return THEMES;
}

}
