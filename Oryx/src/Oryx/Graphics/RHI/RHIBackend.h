#pragma once

#include "Oryx/Graphics/RHI/RHIDeclarations.h"
#include "Oryx/Core/Base.h"

namespace oryx
{

[[nodiscard]] std::string to_string(RHIBackend backend);
[[nodiscard]] RHIBackend parse_rhi_backend(std::string_view str);
[[nodiscard]] RHIBackend default_rhi_backend();

} // namespace oryx
