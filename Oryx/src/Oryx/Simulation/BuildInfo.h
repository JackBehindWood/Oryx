#pragma once

#include "Oryx/Core/Base.h"

namespace oryx
{

struct BuildInfo
{
    std::string version;
    std::string git_hash;
    std::string profile;
    std::string compiler;
    std::string platform;
};

[[nodiscard]] BuildInfo build_info();

} // namespace oryx
