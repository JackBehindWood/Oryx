#include "oxpch.h"
#include "PythonHost.h"

namespace oryx::python
{
namespace { bool g_embedded_host = false; }

void mark_embedded_host() { g_embedded_host = true; }
bool is_embedded_host() { return g_embedded_host; }

} // namespace oryx::python
