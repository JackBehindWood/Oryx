#pragma once

#include "Oryx/Interface/GUI/GuiWidgets.h"

namespace oryx::gui
{

// Debug panel for the GUI itself, in the manner of Unity's UI Debugger: this frame's counters, the item under the pointer with its rect, a toggle that outlines that rect on the tooltip channel
// and one that writes the box tree to the log once when switched on. Place it in any stack; the toggles live in widget state under the name.
void inspector(std::string_view name = "inspector", const WidgetOptions& options = {});

} // namespace oryx::gui
