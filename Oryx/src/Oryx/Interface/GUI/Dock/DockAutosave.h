#pragma once

#include "Oryx/Interface/GUI/Dock/DockOps.h"
#include "Oryx/Interface/GUI/Dock/DockYaml.h"

namespace oryx::gui
{

struct DockFileLoad
{
    DockLayout layout;
    // False when the file came from a newer build: it must never be overwritten.
    bool autosave_allowed = true;
    // True when the layout was read from the file, so it is the user's and not the default.
    bool from_file = false;
    // The file's extension sections (see LayoutExtensions); empty without a readable file.
    LayoutExtensions extensions;
};

// Missing file: `fallback`, silently. Corrupt or invalid file: logged, moved aside to `<file>.bad`, `fallback`. Newer version: logged, `fallback`, autosave off. Never throws.
[[nodiscard]] DockFileLoad load_layout_file(const std::filesystem::path& file, const PanelTable& panels, const DockLayout& fallback);

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
    // `extensions` are written with the layout; mark_dirty says they changed.
    bool update(const DockLayout& layout, bool interacting, const std::filesystem::path& file, const PanelTable& panels, const LayoutExtensions* extensions = nullptr);
    bool flush(const DockLayout& layout, const std::filesystem::path& file, const PanelTable& panels, const LayoutExtensions* extensions = nullptr);
    void mark_dirty() { m_dirty = true; }

    [[nodiscard]] bool pending() const { return m_dirty || m_unsaved; }

private:
    bool write(const DockLayout& layout, const std::filesystem::path& file, const PanelTable& panels, const LayoutExtensions* extensions);

    DockLayout m_seen;
    bool m_allowed;
    bool m_dirty = false;
    bool m_unsaved = false;
    bool m_failure_logged = false;
};

} // namespace oryx::gui
