#pragma once

#include "Oryx/Interface/GUI/Dock/DockOps.h"
#include "Oryx/Interface/GUI/Dock/DockYaml.h"

namespace oryx::gui
{

// Writes a layout file as soon as a change is complete: the frame it happens, or the frame a held drag or splitter is released. No timers; at exit flush covers a write that failed.
// Never throws; a failed write is logged once and retried on the next change or flush.
class DockAutosave
{
public:
    // `initial` is what the file holds (or the default); `allowed` is false for a file from a newer build, which must never be overwritten.
    DockAutosave(const DockLayout& initial, bool allowed);

    // Adopts a layout the user did not ask for (a session-only override) without scheduling a save.
    void rebase(const DockLayout& layout);

    // Call once per frame after the host; it compares against the last layout seen and writes only on a change. Returns true when it wrote the file.
    bool update(const DockLayout& layout, bool interacting, const std::filesystem::path& file, const PanelTable& panels);
    bool flush(const DockLayout& layout, const std::filesystem::path& file, const PanelTable& panels);

    [[nodiscard]] bool pending() const { return m_dirty || m_unsaved; }

private:
    bool write(const DockLayout& layout, const std::filesystem::path& file, const PanelTable& panels);

    DockLayout m_seen;
    bool m_allowed;
    bool m_dirty = false;
    bool m_unsaved = false;
    bool m_failure_logged = false;
};

} // namespace oryx::gui
