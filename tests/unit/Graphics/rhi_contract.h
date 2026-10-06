#pragma once

#include "Oryx.h"

namespace oryx::test
{

// Backend-neutral behaviour every IRHI must honour; live counts are compared as deltas so other devices don't interfere.
void run_rhi_contract(IRHI& rhi);

} // namespace oryx::test
