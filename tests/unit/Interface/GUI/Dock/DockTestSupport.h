#pragma once

#include "doctest.h"

#include "Oryx.h"
#include "Oryx/Interface/GUI/Dock/DockOps.h"
#include "Oryx/Interface/GUI/Dock/DockSolve.h"

namespace oryx::test
{

inline gui::PanelId pid(std::string_view name) { return gui::make_panel_id(name); }

// Views "a".."j" (min 40x30), the viewport "vp" (min 100x80) and "p", a view with the pinned flag set.
inline gui::PanelTable make_panels()
{
    gui::PanelTable table;
    for (char c = 'a'; c <= 'j'; ++c)
        REQUIRE(gui::add_panel(table, std::string_view(&c, 1), std::string_view(&c, 1), gui::PanelKind::View, 40.0f, 30.0f));
    REQUIRE(gui::add_panel(table, "vp", "Viewport", gui::PanelKind::Viewport, 100.0f, 80.0f));
    REQUIRE(gui::add_panel(table, "p", "Pinned", gui::PanelKind::View, 40.0f, 30.0f));
    gui::find_panel(table, pid("p"))->flags = gui::pinned(gui::default_flags(gui::PanelKind::View));
    return table;
}

inline void set_flags(gui::PanelTable& table, std::string_view name, uint8_t bits) { gui::find_panel(table, pid(name))->flags = gui::PanelFlags{ bits }; }

inline void require_applied(const gui::DockResult& result)
{
    REQUIRE(result.applied);
    REQUIRE(result.reason == gui::DockReason::None);
}

// Nodes in depth-first order: 0 split h (ratio 0.7), 1 tabs [a b*], 2 tabs [vp*].
inline gui::DockLayout make_sample(const gui::PanelTable& panels)
{
    gui::DockLayout layout;
    require_applied(gui::dock_panel(layout, panels, pid("a"), gui::k_dock_root, gui::DropZone::Centre));
    require_applied(gui::dock_panel(layout, panels, pid("b"), gui::k_dock_root, gui::DropZone::Centre));
    require_applied(gui::dock_panel(layout, panels, pid("vp"), gui::k_dock_root, gui::DropZone::Right));
    return layout;
}

// Marks a model's panels and layout as the loaded state, so the host never reads a file or rebuilds a default over them.
inline void settle_dock(gui::DockModel& model)
{
    model.load = gui::DockLoad::DefaultOnly;
    model.default_layout = model.layout;
    gui::reset(model.history, model.layout);
    model.history_seeded = true;
}

inline void set_dock_style(const std::function<void(gui::DockStyle&)>& edit)
{
    update_settings<GuiSettings>([&edit](GuiSettings& settings) { edit(settings.dock); });
}

// Deterministic generator so fuzz failures reproduce.
struct Lcg
{
    uint32_t state = 12345u;

    uint32_t next()
    {
        state = state * 1664525u + 1013904223u;
        return state >> 8;
    }

    uint32_t below(uint32_t limit) { return next() % limit; }
    float unit() { return static_cast<float>(next() & 0xFFFFu) / 65535.0f; }
};

}
