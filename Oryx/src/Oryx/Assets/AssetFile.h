#pragma once

#include "Oryx/Core/Base.h"

namespace oryx
{

[[nodiscard]] std::vector<uint8_t> read_binary_file(const std::filesystem::path& path);

} // namespace oryx
