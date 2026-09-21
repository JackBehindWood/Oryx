#include "oxpch.h"
#include "Oryx/Scripting/Support/InitGuard.h"

#include "Oryx/Core/Error.h"

namespace oryx
{

void throw_not_initialised(std::string_view function_name)
{
    throw Error(std::string(function_name) + "() was called before Oryx was initialised");
}

} // namespace oryx
