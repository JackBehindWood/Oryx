#pragma once

#include "Oryx/Core/Base.h"

namespace oryx
{

enum class ScriptSourceKind
{
    File,
    Module
};

struct ScriptSource
{
    ScriptSourceKind kind = ScriptSourceKind::File;
    std::string target;
    std::string language;
    // The directory a file script is imported relative to; empty for a module.
    std::string root;
};

inline bool operator==(const ScriptSource& a, const ScriptSource& b)
{
    return a.kind == b.kind && a.target == b.target && a.language == b.language && a.root == b.root;
}

inline bool operator!=(const ScriptSource& a, const ScriptSource& b)
{
    return !(a == b);
}

} // namespace oryx
