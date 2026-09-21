#pragma once

#include <Python.h>

namespace oryx::python
{

// Owning reference to a Python object; every copy, move-from and destruction needs the GIL (held by the caller).
class PyRef
{
public:
    PyRef() = default;

    [[nodiscard]] static PyRef steal(PyObject* object) { return PyRef(object); }

    [[nodiscard]] static PyRef borrow(PyObject* object)
    {
        Py_XINCREF(object);
        return PyRef(object);
    }

    PyRef(const PyRef& other)
        : m_object(other.m_object)
    {
        Py_XINCREF(m_object);
    }

    PyRef(PyRef&& other) noexcept
        : m_object(std::exchange(other.m_object, nullptr))
    {
    }

    PyRef& operator=(PyRef other) noexcept
    {
        std::swap(m_object, other.m_object);
        return *this;
    }

    ~PyRef() { Py_XDECREF(m_object); }

    [[nodiscard]] PyObject* get() const { return m_object; }
    [[nodiscard]] explicit operator bool() const { return m_object != nullptr; }

    // Gives the reference up without a decref (for objects that must outlive the interpreter).
    PyObject* release() { return std::exchange(m_object, nullptr); }

private:
    explicit PyRef(PyObject* object)
        : m_object(object)
    {
    }

    PyObject* m_object = nullptr;
};

} // namespace oryx::python
