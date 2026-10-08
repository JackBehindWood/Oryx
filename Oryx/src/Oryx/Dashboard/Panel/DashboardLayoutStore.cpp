#include "oxpch.h"
#include "Oryx/Dashboard/Panel/DashboardLayoutStore.h"

#include "Oryx/Dashboard/Panel/DashboardDefaultLayout.h"

namespace oryx
{

DashboardLayoutLoad load_dashboard_layout(const DashboardSettings& settings, const gui::PanelTable& panels)
{
    DashboardLayoutLoad result;
    gui::DockLayout loaded;
    gui::DockYamlStatus status = gui::DockYamlStatus::Ok;
    if (gui::load_layout_yaml(loaded, panels, settings.layout_file, &status))
    {
        result.layout = loaded;
        return result;
    }
    result.layout = default_dashboard_layout(panels);
    if (status == gui::DockYamlStatus::NewerVersion)
        result.autosave_allowed = false;
    else if (status == gui::DockYamlStatus::Corrupt || status == gui::DockYamlStatus::Invalid)
        static_cast<void>(gui::quarantine_layout_file(settings.layout_file));
    return result;
}

bool save_dashboard_layout(const DashboardSettings& settings, const gui::DockLayout& layout, const gui::PanelTable& panels)
{
    return gui::save_layout_yaml(layout, panels, settings.layout_file);
}

} // namespace oryx
