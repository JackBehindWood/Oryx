#include "oxpch.h"
#include "Oryx/Interface/GUI/Dock/DockAutosave.h"

namespace oryx::gui
{

DockFileLoad load_layout_file(const std::filesystem::path& file, const PanelTable& panels, const DockLayout& fallback)
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
    result.layout = fallback;
    if (status == DockYamlStatus::NewerVersion)
        result.autosave_allowed = false;
    else if (status == DockYamlStatus::Corrupt || status == DockYamlStatus::Invalid)
        static_cast<void>(quarantine_layout_file(file));
    return result;
}

DockAutosave::DockAutosave(const DockLayout& initial, bool allowed)
    : m_seen(initial)
    , m_allowed(allowed)
{
}

void DockAutosave::rebase(const DockLayout& layout)
{
    m_seen = layout;
}

bool DockAutosave::update(const DockLayout& layout, bool interacting, const std::filesystem::path& file, const PanelTable& panels)
{
    if (!equal(layout, m_seen))
    {
        m_seen = layout;
        m_dirty = true;
    }
    return m_dirty && !interacting && write(layout, file, panels);
}

bool DockAutosave::flush(const DockLayout& layout, const std::filesystem::path& file, const PanelTable& panels)
{
    if (!equal(layout, m_seen))
    {
        m_seen = layout;
        m_dirty = true;
    }
    return pending() && write(layout, file, panels);
}

bool DockAutosave::write(const DockLayout& layout, const std::filesystem::path& file, const PanelTable& panels)
{
    m_dirty = false;
    if (!m_allowed)
    {
        m_unsaved = false;
        return false;
    }
    if (!save_layout_yaml(layout, panels, file))
    {
        m_unsaved = true;
        if (!m_failure_logged)
        {
            m_failure_logged = true;
            OX_WARN("Could not save the dock layout to '{}'; it is retried on the next change", file.string());
        }
        return false;
    }
    m_unsaved = false;
    m_failure_logged = false;
    return true;
}

} // namespace oryx::gui
