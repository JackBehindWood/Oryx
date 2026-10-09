#include "oxpch.h"
#include "Oryx/Interface/GUI/GuiSettings.h"

namespace oryx
{

namespace
{

template <typename T>
struct Choice
{
    std::string_view name;
    T value;
};

template <typename T, size_t N>
T choose(const SettingsNode& node, std::string_view key, T fallback, const Choice<T> (&choices)[N])
{
    if (!node.has(key))
    {
        return fallback;
    }
    const std::string text = node.string(key);
    std::string allowed;
    for (const Choice<T>& choice : choices)
    {
        if (text == choice.name)
        {
            return choice.value;
        }
        allowed += (allowed.empty() ? "" : ", ") + std::string(choice.name);
    }
    throw SettingsError("gui." + std::string(key) + " must be one of " + allowed + ", got '" + text + "'");
}

} // namespace

void read_settings(GuiSettings& settings, const SettingsNode& node)
{
    settings.docking = node.boolean("docking", settings.docking);
    settings.layout_file = node.has("layout_file") && node.string("layout_file").empty() ? std::filesystem::path() : node.path("layout_file", settings.layout_file);
    gui::DockStyle& dock = settings.dock;
    dock.toolbars = node.boolean("dock_toolbars", dock.toolbars);
    dock.toolbar_placement = choose<gui::ToolbarPlacement>(node, "dock_toolbar_placement", dock.toolbar_placement,
        { { "auto", gui::ToolbarPlacement::Auto }, { "above_tabs", gui::ToolbarPlacement::AboveTabs }, { "below_tabs", gui::ToolbarPlacement::BelowTabs } });
    dock.tab_position = choose<gui::TabPosition>(node, "dock_tab_position", dock.tab_position,
        { { "top", gui::TabPosition::Top }, { "bottom", gui::TabPosition::Bottom } });
    dock.close_buttons = choose<gui::CloseButtons>(node, "dock_close_buttons", dock.close_buttons,
        { { "on_hover", gui::CloseButtons::OnHover }, { "always", gui::CloseButtons::Always }, { "never", gui::CloseButtons::Never } });
    dock.compact = node.boolean("dock_compact", dock.compact);
    dock.guides = node.boolean("dock_guides", dock.guides);
    dock.drop_flash = node.boolean("dock_drop_flash", dock.drop_flash);
    const int64_t opacity = node.integer("dock_preview_opacity", static_cast<int64_t>(std::lround(dock.preview_opacity * 100.0f)));
    if (opacity < 0 || opacity > 100)
    {
        throw SettingsError("gui.dock_preview_opacity must be between 0 and 100, got " + std::to_string(opacity));
    }
    dock.preview_opacity = static_cast<float>(opacity) / 100.0f;
}

} // namespace oryx

OX_REGISTER_SETTINGS(oryx::GuiSettings, "gui")
