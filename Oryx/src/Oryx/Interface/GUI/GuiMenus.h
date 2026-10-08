#pragma once

#include "Oryx/Interface/GUI/GuiControls.h"

// Menus, popups and the overlays that float above the rest: they draw on the reserved popup and tooltip channels and, except for menu items and popup contents, never take input.
namespace oryx::gui
{

// A bar across the stack it sits in that holds menus; close it with end_menu_bar.
void begin_menu_bar(std::string_view name, const WidgetOptions& options = {});
void end_menu_bar();

// A menu header, in a bar or inside another menu (a submenu opens to the side on hover). Returns whether its list is open; fill it and call end_menu only when true.
// Clicking a header toggles it; with one open, hovering a sibling header switches to it. Escape or a press outside closes the innermost open menu.
[[nodiscard]] bool begin_menu(std::string_view label, const WidgetOptions& options = {});
void end_menu();
// A row of an open menu; a click closes every open menu in the next frame.
[[nodiscard]] ItemState menu_item(std::string_view label, bool selected = false, const WidgetOptions& options = {});

class MenuBarScope
{
public:
    explicit MenuBarScope(std::string_view name, const WidgetOptions& options = {}) { begin_menu_bar(name, options); }
    ~MenuBarScope() { end_menu_bar(); }

    MenuBarScope(const MenuBarScope&) = delete;
    MenuBarScope& operator=(const MenuBarScope&) = delete;
};

class MenuScope
{
public:
    explicit MenuScope(std::string_view label, const WidgetOptions& options = {})
        : m_open(begin_menu(label, options))
    {
    }
    ~MenuScope()
    {
        if (m_open)
        {
            end_menu();
        }
    }

    MenuScope(const MenuScope&) = delete;
    MenuScope& operator=(const MenuScope&) = delete;

    [[nodiscard]] bool open() const { return m_open; }

private:
    bool m_open;
};

// A popup that opens where the pointer was when open_popup ran and stays until close_popup, a press outside or Escape. begin_popup returns whether it is open; call end_popup only then.
void open_popup(std::string_view name);
// Same scope as open_popup; from inside a popup's own contents use close_current_popup.
void close_popup(std::string_view name);
void close_current_popup();
[[nodiscard]] bool begin_popup(std::string_view name, const WidgetOptions& options = {});
void end_popup();

// Opens the popup `name` on a right click of the item and returns whether it is open, like begin_popup.
[[nodiscard]] bool context_menu(const ItemState& item, std::string_view name, const WidgetOptions& options = {});

// Shows `text` at the pointer once the item has been hovered for the delay (negative: the theme's tooltip_delay). It never takes input.
void tooltip(const ItemState& item, std::string_view text, float delay = -1.0f);

// Queues a message for show_toasts.
void toast(std::string_view text, float seconds = 3.0f);
// Call once per frame after the rest: draws the queued toasts bottom-right and ages them by the frame's delta time.
void show_toasts();

} // namespace oryx::gui
