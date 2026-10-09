#include "oxpch.h"
#include "Oryx/Interface/GUI/Dock/GuiDockTheme.h"

namespace oryx::gui
{

namespace
{

Colour with_alpha(Colour colour, float alpha)
{
    colour.a = alpha;
    return colour;
}

}

GuiDockTheme derive_dock_theme(const GuiTheme& theme)
{
    const Colour accent = theme.panel.accent;
    GuiDockTheme dock;
    dock.preview = with_alpha(accent, 0.28f);
    dock.preview_border = accent;
    dock.guide = with_alpha(theme.overlay.background, 0.88f);
    dock.guide_border = accent;
    dock.guide_hover = with_alpha(accent, 0.95f);
    dock.guide_glyph = accent;
    dock.guide_glyph_hover = theme.overlay.background;
    dock.refusal = theme.palette[5];
    dock.chip_border = accent;
    dock.focus_ring = accent;
    dock.landing_flash = accent;
    return dock;
}

}
