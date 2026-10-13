#pragma once

#include "Oryx/Interface/GUI/GuiWidgets.h"

namespace oryx::gui
{

// A live tour of the widgets, drawn into the current panel of the active context the way ImGui's ShowDemoWindow is: call it every frame from the body of one panel. Its state sits in the context's widget-state table,
// so it starts fresh after a frame in which it was not drawn; it reads the theme but never changes it.
void show_showcase();

} // namespace oryx::gui
