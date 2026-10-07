#pragma once

#include "Oryx/Interface/Canvas/ImTheme.h"

namespace oryx
{

// The shared theme plus the roles only developer tooling needs.
struct GuiTheme : ImTheme
{
    // Space between neighbouring widgets in a row or column.
    float spacing = 4.0f;
};

} // namespace oryx
