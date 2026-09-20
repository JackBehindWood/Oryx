#pragma once

#include "Oryx/Game/IState.h"
#include "Oryx/Scripting/ScriptOrigin.h"

namespace oryx
{

class IScriptedState : public IState
{
public:
    virtual const ScriptOrigin& origin() const = 0;
};

} // namespace oryx
