#pragma once

#include "Oryx/Interface/GUI/Dock/DockLayout.h"

namespace oryx
{

inline constexpr const char* k_viewport_panel = "viewport/0";
inline constexpr const char* k_probabilities_panel = "view/probabilities";
inline constexpr const char* k_values_panel = "view/values";

// The board beside a column of tabbed dashboard views; a panel missing from the table is left out, so the result is valid for any subset.
[[nodiscard]] gui::DockLayout default_dashboard_layout(const gui::PanelTable& panels);

} // namespace oryx
