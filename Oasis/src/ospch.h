#pragma once

// Oasis's own PCH: just forwards to Oryx's, since Oasis code needs the same
// ambient std/Oryx-core includes Oryx.h consumers rely on (oxpch.h). Kept as
// its own named header/source pair (rather than reusing oxpch.h directly as
// Oasis's pchheader) so Oasis's precompiled header is distinct from Oryx's.
#include "oxpch.h"
