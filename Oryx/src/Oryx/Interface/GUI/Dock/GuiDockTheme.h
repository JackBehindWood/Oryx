#pragma once

#include "Oryx/Interface/GUI/GuiTheme.h"

// Every colour the dock draws beyond the panel and tab roles. Derived from the active GuiTheme so the three themes stay consistent; the context owns it next to GuiTheme.
namespace oryx::gui
{

struct GuiDockTheme
{
    Colour preview;
    Colour preview_border;
    Colour guide;
    Colour guide_border;
    Colour guide_hover;
    Colour guide_glyph;
    Colour guide_glyph_hover;
    Colour refusal;
    Colour chip_border;
    Colour focus_ring;
    Colour landing_flash;
};

static_assert(std::is_trivially_copyable_v<GuiDockTheme>);

[[nodiscard]] GuiDockTheme derive_dock_theme(const GuiTheme& theme);

}
