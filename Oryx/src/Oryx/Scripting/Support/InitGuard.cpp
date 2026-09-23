#include "oxpch.h"
#include "Oryx/Scripting/Support/InitGuard.h"

namespace oryx
{

void throw_not_initialised(std::string_view function_name)
{
    throw NotInitialisedError(std::string(function_name) + "() was called before Oryx was initialised");
}

} // namespace oryx
