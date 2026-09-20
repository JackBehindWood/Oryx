#pragma once

#include "Oryx/Core/Params.h"
#include "Oryx/Game/IGame.h"
#include "Oryx/Scripting/ScriptOrigin.h"

namespace oryx
{

class IScriptedGame : public IGame
{
public:
    virtual const ScriptOrigin& origin() const = 0;
    virtual const ParamSchema& param_schema() const = 0;
};

} // namespace oryx
