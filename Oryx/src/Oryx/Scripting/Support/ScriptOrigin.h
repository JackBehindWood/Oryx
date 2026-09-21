#pragma once

#include "Oryx/Core/Base.h"

namespace oryx
{

struct ScriptOrigin
{
    std::string language;
    std::string module;
    std::string source_file;
};

inline bool operator==(const ScriptOrigin& a, const ScriptOrigin& b)
{
    return a.language == b.language && a.module == b.module && a.source_file == b.source_file;
}

inline bool operator!=(const ScriptOrigin& a, const ScriptOrigin& b)
{
    return !(a == b);
}

} // namespace oryx
