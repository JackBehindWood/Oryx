#pragma once

#include "Oryx/Dashboard/Feed/DashboardModel.h"
#include "Oryx/Interface/GUI/Dock/DockLayout.h"

namespace oryx
{

// Where a view's panel first docks: views sharing a side are tabbed together, the first of them docks at `size` points, beside the board or (not `beside_board`) along the whole window edge.
struct DashboardViewPlacement
{
    gui::DropZone side = gui::DropZone::Right;
    float size = 380.0f;
    bool beside_board = true;
};

// A dashboard pane: draws its body with the active GuiContext; the host owns the header, size and placement.
class IDashboardView
{
public:
    virtual ~IDashboardView() = default;

    [[nodiscard]] virtual std::string_view title() const = 0;
    [[nodiscard]] virtual DashboardViewPlacement placement() const { return {}; }
    virtual void draw(DashboardModel& model) = 0;
};

} // namespace oryx
