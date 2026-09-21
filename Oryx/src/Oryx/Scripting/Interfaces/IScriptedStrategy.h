#pragma once

#include "Oryx/Scripting/Support/ScriptOrigin.h"
#include "Oryx/Strategy/IStrategy.h"

namespace oryx
{

class IScriptedStrategy : public IStrategy
{
public:
    virtual const ScriptOrigin& origin() const = 0;
};

} // namespace oryx
