#pragma once

#include "Oryx/Dashboard/Feed/DashboardModel.h"

namespace oryx
{

// A dashboard pane: draws its body with the active GuiContext; the host owns the header, size and placement.
class IDashboardView
{
public:
    virtual ~IDashboardView() = default;

    [[nodiscard]] virtual std::string_view title() const = 0;
    virtual void draw(DashboardModel& model) = 0;
};

} // namespace oryx
