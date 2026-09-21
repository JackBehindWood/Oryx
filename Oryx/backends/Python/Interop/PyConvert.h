#pragma once

#include "Interop/PyRef.h"

#include "Oryx/Game/ActionId.h"
#include "Oryx/Game/PlayerId.h"

namespace oryx::python
{

// Conversions between C++ values and Python objects through the C API; from_py returns false, leaving no Python error set, when the object does not fit.
template<typename T>
struct PyConvert;

template<>
struct PyConvert<PyRef>
{
    static constexpr const char* expected = "an object";
    static PyRef to_py(const PyRef& value) { return value; }
    static bool from_py(PyObject* object, PyRef& out)
    {
        out = PyRef::borrow(object);
        return true;
    }
};

template<typename Integer>
struct PyIntegerConvert
{
    static_assert(std::is_integral_v<Integer>);
    static constexpr const char* expected = "an int";

    static PyRef to_py(Integer value)
    {
        if constexpr (std::is_signed_v<Integer>)
        {
            return PyRef::steal(PyLong_FromLongLong(value));
        }
        else
        {
            return PyRef::steal(PyLong_FromUnsignedLongLong(value));
        }
    }

    static bool from_py(PyObject* object, Integer& out)
    {
        PyRef index = PyRef::steal(PyNumber_Index(object));
        if (!index)
        {
            PyErr_Clear();
            return false;
        }

        if constexpr (std::is_signed_v<Integer>)
        {
            long long value = PyLong_AsLongLong(index.get());
            if (value == -1 && PyErr_Occurred() != nullptr)
            {
                PyErr_Clear();
                return false;
            }
            if (value < std::numeric_limits<Integer>::min() || value > std::numeric_limits<Integer>::max())
            {
                return false;
            }
            out = static_cast<Integer>(value);
        }
        else
        {
            unsigned long long value = PyLong_AsUnsignedLongLong(index.get());
            if (value == static_cast<unsigned long long>(-1) && PyErr_Occurred() != nullptr)
            {
                PyErr_Clear();
                return false;
            }
            if (value > std::numeric_limits<Integer>::max())
            {
                return false;
            }
            out = static_cast<Integer>(value);
        }
        return true;
    }
};

template<>
struct PyConvert<uint32_t> : PyIntegerConvert<uint32_t>
{
};

template<>
struct PyConvert<int32_t> : PyIntegerConvert<int32_t>
{
};

template<>
struct PyConvert<bool>
{
    static constexpr const char* expected = "a bool";

    static PyRef to_py(bool value) { return PyRef::borrow(value ? Py_True : Py_False); }

    static bool from_py(PyObject* object, bool& out)
    {
        PyNumberMethods* number = Py_TYPE(object)->tp_as_number;
        if (object != Py_True && object != Py_False && (number == nullptr || number->nb_bool == nullptr))
        {
            return false;
        }

        int truth = PyObject_IsTrue(object);
        if (truth < 0)
        {
            PyErr_Clear();
            return false;
        }
        out = truth != 0;
        return true;
    }
};

template<>
struct PyConvert<std::string>
{
    static constexpr const char* expected = "a str";

    static PyRef to_py(const std::string& value) { return PyRef::steal(PyUnicode_FromStringAndSize(value.data(), static_cast<Py_ssize_t>(value.size()))); }

    static bool from_py(PyObject* object, std::string& out)
    {
        if (PyUnicode_Check(object) == 0)
        {
            return false;
        }

        Py_ssize_t size = 0;
        const char* text = PyUnicode_AsUTF8AndSize(object, &size);
        if (text == nullptr)
        {
            PyErr_Clear();
            return false;
        }
        out.assign(text, static_cast<size_t>(size));
        return true;
    }
};

template<typename Element, typename Container>
struct PySequenceConvert
{
    static bool from_py(PyObject* object, Container& out)
    {
        PyRef sequence = PyRef::steal(PySequence_Fast(object, ""));
        if (!sequence)
        {
            PyErr_Clear();
            return false;
        }

        Py_ssize_t size = PySequence_Fast_GET_SIZE(sequence.get());
        out.clear();
        out.reserve(static_cast<size_t>(size));
        for (Py_ssize_t i = 0; i < size; ++i)
        {
            Element element{};
            if (!PyConvert<Element>::from_py(PySequence_Fast_GET_ITEM(sequence.get(), i), element))
            {
                return false;
            }
            out.push_back(element);
        }
        return true;
    }
};

template<>
struct PyConvert<ActionList> : PySequenceConvert<ActionId, ActionList>
{
    static constexpr const char* expected = "a sequence of ints";
};

template<>
struct PyConvert<double>
{
    static constexpr const char* expected = "a number";

    static PyRef to_py(double value) { return PyRef::steal(PyFloat_FromDouble(value)); }

    static bool from_py(PyObject* object, double& out)
    {
        double value = PyFloat_AsDouble(object);
        if (value == -1.0 && PyErr_Occurred() != nullptr)
        {
            PyErr_Clear();
            return false;
        }
        out = value;
        return true;
    }
};

template<>
struct PyConvert<std::vector<double>> : PySequenceConvert<double, std::vector<double>>
{
    static constexpr const char* expected = "a sequence of numbers";
};

static_assert(std::is_same_v<ActionId, uint32_t>, "PyConvert<uint32_t> is the ActionId conversion");
static_assert(std::is_same_v<PlayerId, int32_t>, "PyConvert<int32_t> is the PlayerId conversion");

} // namespace oryx::python
