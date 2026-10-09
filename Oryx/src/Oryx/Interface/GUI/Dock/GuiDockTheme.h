#pragma once

#include "Oryx/Interface/GUI/GuiTheme.h"

// Every colour the dock draws beyond the panel and tab roles. Derived from the active GuiTheme so the three themes stay consistent; the context owns it next to GuiTheme.
namespace oryx::gui
{

// Where each token comes from (derive_dock_theme); assign a field after set_theme to override one. A = theme.panel.accent, O = theme.overlay.background.
struct GuiDockTheme
{
    // A at 28%: the fill of the area a drop would take.
    Colour preview;
    // A.
    Colour preview_border;
    // O at 88%: an inner guide at rest.
    Colour guide;
    Colour guide_border;
    // A at 95%: the hovered inner guide.
    Colour guide_hover;
    // A: the mark inside a guide at rest.
    Colour guide_glyph;
    // O: the mark inside a hovered guide.
    Colour guide_glyph_hover;
    // panel.border at 90%: a guide (and the dragged tab's outline) where dropping changes nothing.
    Colour here_state;
    // palette[1] (orange, so a window-edge guide never looks like a split-this-panel one) at 85% and 100%: the guides that dock beside the whole tree, at rest and hovered.
    Colour outer_guide;
    Colour outer_guide_hover;
    // palette[5] (vermillion): a refused target and its label border.
    Colour refusal;
    // A: the ghost chip's border.
    Colour chip_border;
    // A: the ring on the focused panel.
    Colour focus_ring;
    // A: the flash on the panel that just landed.
    Colour landing_flash;
};

static_assert(std::is_trivially_copyable_v<GuiDockTheme>);

[[nodiscard]] GuiDockTheme derive_dock_theme(const GuiTheme& theme);

}
