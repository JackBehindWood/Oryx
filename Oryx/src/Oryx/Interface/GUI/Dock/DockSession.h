#pragma once

#include "Oryx/Interface/GUI/Dock/DockAutosave.h"
#include "Oryx/Interface/GUI/Dock/GuiPanelHost.h"

// Everything a dock needs besides drawing: the layout, its default, the file it lives in, autosave and group visibility. A consumer registers panels with hints and then only draws bodies.
namespace oryx::gui
{

struct DockFileLoad
{
    DockLayout layout;
    // False when the file came from a newer build: it must never be overwritten.
    bool autosave_allowed = true;
    // True when the layout was read from the file, so it is the user's and not the default.
    bool from_file = false;
};

// Missing file: the default, silently. Corrupt or invalid file: logged, moved aside to `<file>.bad`, the default. Newer version: logged, the default, autosave off. Never throws.
[[nodiscard]] DockFileLoad load_layout_file(const std::filesystem::path& file, const PanelTable& panels);

// Built over the active GuiContext after its panels are registered; calls that touch the host (toggle, update, flush, menus) need that context active. The file is `gui.layout_file` and the look is `gui.dock_*`, both read live.
class DockSession
{
public:
    DockSession();

    [[nodiscard]] DockLayout& layout() { return m_layout; }
    [[nodiscard]] const DockLayout& layout() const { return m_layout; }
    [[nodiscard]] const DockLayout& default_layout() const { return m_default; }
    [[nodiscard]] PanelHostOptions host_options() const;

    // True when the layout came from the file and not the default.
    [[nodiscard]] bool from_file() const { return m_from_file; }

    void toggle_panel(PanelId panel);
    [[nodiscard]] bool group_open(std::string_view group) const;
    // A user edit: undoable and autosaved.
    void set_group_open(std::string_view group, bool open);
    // A session edit (a command-line flag): applied to the layout and to the default, never autosaved.
    void override_group_open(std::string_view group, bool open);
    void reset_layout();

    // Call once per frame after the host; writes the file the frame a change completes.
    void update();
    void flush();

private:
    DockLayout m_layout;
    DockLayout m_default;
    // The context's table outlives the session and stays readable outside a frame.
    const PanelTable* m_panels;
    UniquePtr<DockAutosave> m_autosave;
    bool m_from_file = false;
};

// Panels list, Reset layout and Undo/Redo layout as items of the menu being built.
void dock_menu(DockSession& session);

// A panel whose body another GuiContext draws into: the region to give that context and whether the host context hides the pointer from it.
class ForeignPanel
{
public:
    explicit ForeignPanel(std::string_view name) : m_name(name) {}

    // Call after the host frame, on the host's context; `pointer` is the frame's pointer position.
    void update(const Vec2f& pointer);

    [[nodiscard]] const Rect& rect() const { return m_rect; }
    [[nodiscard]] bool visible() const { return !is_empty(m_rect); }
    // A float or popup of the host is over the panel, so the other context should not take the pointer.
    [[nodiscard]] bool blocked() const { return m_blocked; }
    // True when the visible panel's context wants input the host must not hand on to the board.
    [[nodiscard]] bool claims_pointer(const GuiContext& other) const { return visible() && (other.wants_mouse() || other.popup_open()); }
    [[nodiscard]] bool claims_keyboard(const GuiContext& other) const { return visible() && (other.wants_keyboard() || other.popup_open()); }

private:
    std::string m_name;
    Rect m_rect;
    bool m_blocked = false;
};

}
