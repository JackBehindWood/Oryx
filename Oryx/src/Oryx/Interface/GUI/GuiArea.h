#pragma once

#include "Oryx/Interface/GUI/GuiControls.h"

// A region of the surface anchored to one of its edges: the one primitive for menu and status bars, toolbars, rails, drawers and the box the dock host fills. Declare areas outside-in, each from what the earlier ones left:
//   if (AreaScope bar("menu", { .anchor = AreaAnchor::Top })) { ... }       // reserves a strip, later areas see the rest
//   if (AreaScope host("dock", { .anchor = AreaAnchor::Fill })) { begin_panel_host(); ... end_panel_host(); }
// An area is a fixed-size box floated at its place, so the content inside lays out as in any column. Throws Error for none of its own; end_area follows only when begin_area returned true.
namespace oryx::gui
{

enum class AreaAnchor : uint8_t
{
    Top,
    Bottom,
    Left,
    Right,
    // What the reserved areas before it left; it takes no input of its own (a viewport inside it still gets the pointer); it leaves the space as it found it, so an Overlay declared after it floats over it.
    Fill
};

enum class AreaMode : uint8_t
{
    // Takes its size off the remaining space, so later areas start behind it.
    Reserve,
    // Floats over the remaining space and takes none, like a drawer. Declare it after the areas it covers: later areas are above earlier ones, in what is drawn and in what the pointer reaches.
    Overlay
};

struct AreaOptions : WidgetOptions
{
    AreaAnchor anchor = AreaAnchor::Fill;
    // The thickness across the anchored edge (the height of Top and Bottom, the width of Left and Right); Fill ignores it. Fixed is points, Percent a share of the remaining space, anything else the size the content took last frame.
    Sizing size = fit();
    AreaMode mode = AreaMode::Reserve;
    // A grip on the inner edge changes the size, kept under the area's name; the Sizing's min and max bound it.
    bool resizable = false;
    // The caller's flag: false hides the area (begin_area returns false and it reserves nothing). An Overlay under the pointer clears it on Escape.
    bool* open = nullptr;
    // The content scrolls vertically instead of being clipped.
    bool scroll = false;
};

static_assert(std::is_trivially_copyable_v<AreaOptions>);

// The size an area with a fit size takes before its content has been measured.
inline constexpr float k_default_area_size = 24.0f;
// The width of the resize grip, in points.
inline constexpr float k_area_grip = 5.0f;

// Returns whether the area is open; fill it and call end_area only then.
[[nodiscard]] bool begin_area(std::string_view name, const AreaOptions& options = {});
void end_area();

// Where the area `name` was solved in the latest frame; empty when it has not been.
[[nodiscard]] Rect area_rect(std::string_view name);
// What the areas declared so far this frame have left of the surface.
[[nodiscard]] Rect remaining_area();

struct AreaEnd
{
    static void end() { end_area(); }
};

class AreaScope : public Scope<AreaEnd>
{
public:
    explicit AreaScope(std::string_view name, const AreaOptions& options = {})
        : Scope(begin_area(name, options))
    {
    }
};

} // namespace oryx::gui
