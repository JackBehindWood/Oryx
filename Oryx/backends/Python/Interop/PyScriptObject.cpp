#include "oxpch.h"
#include "Interop/PyScriptObject.h"

namespace oryx::python
{

namespace
{

int64_t g_live_script_objects = 0;

} // namespace

int64_t live_script_objects()
{
    return g_live_script_objects;
}

PyScriptObject::PyScriptObject()
{
    ++g_live_script_objects;
}

PyScriptObject::PyScriptObject(PyRef object)
    : m_object(std::move(object))
{
    ++g_live_script_objects;
}

PyScriptObject::~PyScriptObject()
{
    --g_live_script_objects;
    if (!m_object)
    {
        return;
    }

    if (Py_IsInitialized() != 0)
    {
        PyGILState_STATE state = PyGILState_Ensure();
        m_object = PyRef();
        PyGILState_Release(state);
    }
    else
    {
        m_object.release();
    }
}

} // namespace oryx::python
