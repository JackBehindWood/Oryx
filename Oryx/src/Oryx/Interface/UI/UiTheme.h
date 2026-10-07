#pragma once

#include "Oryx/Interface/Canvas/ImTheme.h"

namespace oryx
{

// The shared theme plus the roles only player-facing UI needs.
struct UiTheme : ImTheme
{
    // Space between neighbouring widgets in a row or column.
    float spacing = 8.0f;
    // The status line carries no fill, so only its text, size and padding apply.
    ImStyle status;
};

} // namespace oryx
