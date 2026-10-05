#pragma once

#include "Oryx/Board/IConsoleBoard.h"
#include "Oryx/Scripting/Support/ScriptOrigin.h"

namespace oryx
{

class IScriptedConsoleBoard : public IConsoleBoard
{
public:
    virtual const ScriptOrigin& origin() const = 0;
};

} // namespace oryx
