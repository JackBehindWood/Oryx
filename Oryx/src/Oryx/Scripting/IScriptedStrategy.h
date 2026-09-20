#pragma once

#include "Oryx/Scripting/ScriptOrigin.h"
#include "Oryx/Strategy/IStrategy.h"

namespace oryx
{

class IScriptedStrategy : public IStrategy
{
public:
    virtual const ScriptOrigin& origin() const = 0;
};

} // namespace oryx
