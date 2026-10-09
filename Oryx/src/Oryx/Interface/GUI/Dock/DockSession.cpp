#include "oxpch.h"
#include "Oryx/Interface/GUI/Dock/DockSession.h"

#include "Oryx/Interface/GUI/GuiSettings.h"
#include "Oryx/Interface/GUI/GuiWidgets.h"

namespace oryx::gui
{

namespace
{

bool has_viewport(const PanelTable& panels)
{
    for (uint32_t i = 0; i < panels.count; ++i)
        if (panels.descs[i].kind == PanelKind::Viewport)
            return true;
    return false;
}

bool viewport_open(const DockLayout& layout, const PanelTable& panels)
{
    for (uint32_t i = 0; i < panels.count; ++i)
        if (panels.descs[i].kind == PanelKind::Viewport && is_open(layout, panels.descs[i].id))
            return true;
    return false;
}

const std::filesystem::path& layout_file()
{
    return settings_of<GuiSettings>().layout_file;
}

}

DockFileLoad load_layout_file(const std::filesystem::path& file, const PanelTable& panels)
{
    DockFileLoad result;
    DockLayout loaded;
    DockYamlStatus status = DockYamlStatus::Ok;
    if (load_layout_yaml(loaded, panels, file, &status))
    {
        result.layout = loaded;
        result.from_file = true;
        return result;
    }
    result.layout = build_default_layout(panels);
    if (status == DockYamlStatus::NewerVersion)
        result.autosave_allowed = false;
    else if (status == DockYamlStatus::Corrupt || status == DockYamlStatus::Invalid)
        static_cast<void>(quarantine_layout_file(file));
    return result;
}

DockSession::DockSession()
    : m_default(build_default_layout(panels()))
    , m_panels(&panels())
{
    DockFileLoad load = load_layout_file(layout_file(), *m_panels);
    const bool usable = !has_viewport(*m_panels) || viewport_open(load.layout, *m_panels);
    m_layout = usable ? load.layout : m_default;
    m_from_file = load.from_file && usable;
    m_autosave = create_unique<DockAutosave>(m_layout, load.autosave_allowed);
    reset_layout_history(m_layout);
}

PanelHostOptions DockSession::host_options() const
{
    PanelHostOptions options;
    options.require_viewport = has_viewport(*m_panels);
    options.default_layout = &m_default;
    options.style = settings_of<GuiSettings>().dock;
    return options;
}

void DockSession::toggle_panel(PanelId panel)
{
    if (is_open(m_layout, panel))
        static_cast<void>(close_panel(m_layout, *m_panels, panel));
    else
        static_cast<void>(open_panel(m_layout, *m_panels, panel));
}

bool DockSession::group_open(std::string_view group) const
{
    return oryx::gui::group_open(m_layout, *m_panels, make_panel_id(group));
}

void DockSession::set_group_open(std::string_view group, bool open)
{
    static_cast<void>(oryx::gui::set_group_open(m_layout, *m_panels, make_panel_id(group), open));
}

void DockSession::override_group_open(std::string_view group, bool open)
{
    static_cast<void>(oryx::gui::set_group_open(m_layout, *m_panels, make_panel_id(group), open));
    static_cast<void>(oryx::gui::set_group_open(m_default, *m_panels, make_panel_id(group), open));
    m_autosave->rebase(m_layout);
    reset_layout_history(m_layout);
}

void DockSession::reset_layout()
{
    m_layout = m_default;
    reset_layout_history(m_layout);
}

void DockSession::update()
{
    m_autosave->update(m_layout, panel_host_result().interacting, layout_file(), *m_panels);
}

void DockSession::flush()
{
    m_autosave->flush(m_layout, layout_file(), *m_panels);
}

void dock_menu(DockSession& session)
{
    if (begin_menu("Panels"))
    {
        const PanelTable& table = panels();
        for (uint32_t index = 0; index < table.count; ++index)
        {
            const PanelDesc& desc = table.descs[index];
            if (desc.kind == PanelKind::Viewport)
                continue;
            const bool open = is_open(session.layout(), desc.id);
            DisabledScope disabled(open && can_close(table, desc.id) != DockReason::None);
            if (menu_item(desc.title, open).clicked)
                session.toggle_panel(desc.id);
        }
        end_menu();
    }
    if (menu_item("Reset layout").clicked)
        session.reset_layout();
    {
        DisabledScope disabled(!can_undo_layout());
        if (menu_item("Undo layout").clicked)
            undo_layout();
    }
    {
        DisabledScope disabled(!can_redo_layout());
        if (menu_item("Redo layout").clicked)
            redo_layout();
    }
}

void ForeignPanel::update(const Vec2f& pointer)
{
    m_rect = panel_rect(m_name);
    m_blocked = panel_occluded(m_name, pointer) || context().popup_open();
}

}
