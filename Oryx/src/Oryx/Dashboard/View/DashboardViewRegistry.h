#pragma once

#include "Oryx/Core/Registry.h"
#include "Oryx/Dashboard/View/IDashboardView.h"

namespace oryx
{

using DashboardViewRegistry = Registry<IDashboardView>;

} // namespace oryx

#define OX_REGISTER_DASHBOARD_VIEW(Type, name, ...) \
    OX_REGISTER_FACTORY(::oryx::IDashboardView, Type, name __VA_OPT__(,) __VA_ARGS__)
