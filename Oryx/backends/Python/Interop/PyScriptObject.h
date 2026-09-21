#pragma once

#include "Interop/PyGil.h"
#include "Interop/PyRef.h"

namespace oryx::python
{

// Number of PyScriptObjects that have not been destroyed yet.
[[nodiscard]] int64_t live_script_objects();

// Owns a Python object from C++: released under the GIL, or leaked once the interpreter has been finalised.
class PyScriptObject
{
public:
    PyScriptObject();
    explicit PyScriptObject(PyRef object);
    ~PyScriptObject();

    PyScriptObject(const PyScriptObject&) = delete;
    PyScriptObject& operator=(const PyScriptObject&) = delete;

    [[nodiscard]] PyObject* get() const { return m_object.get(); }

private:
    PyRef m_object;
};

} // namespace oryx::python
