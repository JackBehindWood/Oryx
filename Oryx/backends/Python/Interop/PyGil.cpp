#include "oxpch.h"
#include "Interop/PyGil.h"

#include "Oryx/Scripting/Support/ScriptError.h"

namespace oryx::python
{

PyGil::PyGil()
{
    if (Py_IsInitialized() == 0)
    {
        throw ScriptError("the Python runtime is not running");
    }
    m_state = PyGILState_Ensure();
}

PyGil::~PyGil()
{
    PyGILState_Release(m_state);
}

} // namespace oryx::python
