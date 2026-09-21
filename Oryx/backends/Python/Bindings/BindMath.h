#pragma once

#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "Oryx/Core/Error.h"

namespace oryx::python
{

// Python names are derived from the C++ type: Vec3 for Vector<3, double>, Vec3f for Vector<3, float>, Mat2 for a 2x2 matrix, Mat2x3 for a 2x3 one.
template<typename T>
const char* scalar_suffix()
{
    if constexpr (std::is_same_v<T, double>)
    {
        return "";
    }
    else if constexpr (std::is_same_v<T, float>)
    {
        return "f";
    }
    else if constexpr (std::is_same_v<T, int32_t>)
    {
        return "i";
    }
    else
    {
        static_assert(sizeof(T) == 0, "give this scalar type a suffix in scalar_suffix()");
    }
}

template<size_t N, typename T>
std::string vector_name()
{
    return "Vec" + std::to_string(N) + scalar_suffix<T>();
}

template<size_t R, size_t C, typename T>
std::string matrix_name()
{
    return "Mat" + std::to_string(R) + (R == C ? "" : "x" + std::to_string(C)) + scalar_suffix<T>();
}

inline size_t wrapped_index(int64_t index, size_t size)
{
    int64_t wrapped = index < 0 ? index + static_cast<int64_t>(size) : index;
    if (wrapped < 0 || wrapped >= static_cast<int64_t>(size))
    {
        throw pybind11::index_error("index out of range");
    }
    return static_cast<size_t>(wrapped);
}

template<typename T>
std::string element_repr(T value)
{
    return pybind11::repr(pybind11::cast(value)).template cast<std::string>();
}

template<size_t, typename T>
using Component = T;

template<size_t N, typename T, size_t... I>
void bind_component_init(pybind11::class_<Vector<N, T>>& type, std::index_sequence<I...>)
{
    type.def(pybind11::init([](Component<I, T>... values) { return Vector<N, T>(values...); }));
}

template<size_t N, typename T>
Vector<N, T> vector_from_sequence(const std::vector<T>& values)
{
    if (values.size() != N)
    {
        throw Error(vector_name<N, T>() + " needs " + std::to_string(N) + " values, got " + std::to_string(values.size()));
    }
    Vector<N, T> result;
    for (size_t i = 0; i < N; ++i)
    {
        result[i] = values[i];
    }
    return result;
}

template<size_t N, typename T>
void bind_vector(pybind11::module_& module)
{
    namespace py = pybind11;
    using V = Vector<N, T>;

    std::string name = vector_name<N, T>();
    py::class_<V> type(module, name.c_str(), py::buffer_protocol(), "A fixed-size vector; also a buffer, so numpy.asarray(v) works.");
    type.def(py::init<>())
        .def(py::init<T>(), py::arg("value"))
        .def(py::init([](const std::vector<T>& values) { return vector_from_sequence<N, T>(values); }), py::arg("values"));
    bind_component_init<N, T>(type, std::make_index_sequence<N>{});

    if constexpr (N >= 1) { type.def_property("x", [](const V& v) { return v[0]; }, [](V& v, T value) { v[0] = value; }); }
    if constexpr (N >= 2) { type.def_property("y", [](const V& v) { return v[1]; }, [](V& v, T value) { v[1] = value; }); }
    if constexpr (N >= 3) { type.def_property("z", [](const V& v) { return v[2]; }, [](V& v, T value) { v[2] = value; }); }
    if constexpr (N >= 4) { type.def_property("w", [](const V& v) { return v[3]; }, [](V& v, T value) { v[3] = value; }); }

    type.def("__len__", [](const V&) { return N; })
        .def("__getitem__", [](const V& v, int64_t index) { return v[wrapped_index(index, N)]; })
        .def("__setitem__", [](V& v, int64_t index, T value) { v[wrapped_index(index, N)] = value; })
        .def("__neg__", [](const V& v) { return -v; })
        .def("__add__", [](const V& a, const V& b) { return a + b; }, py::is_operator())
        .def("__sub__", [](const V& a, const V& b) { return a - b; }, py::is_operator())
        .def("__mul__", [](const V& v, T scalar) { return v * scalar; }, py::is_operator())
        .def("__mul__", [](const V& a, const V& b) { return a * b; }, py::is_operator())
        .def("__rmul__", [](const V& v, T scalar) { return v * scalar; }, py::is_operator())
        .def("__truediv__", [](const V& v, T scalar) { return v / scalar; }, py::is_operator())
        .def("__truediv__", [](const V& a, const V& b) { return a / b; }, py::is_operator())
        .def("__eq__", [](const V& a, const V& b) { return a == b; }, py::is_operator())
        .def("__ne__", [](const V& a, const V& b) { return a != b; }, py::is_operator())
        .def("length", &V::length)
        .def("length_squared", &V::length_squared)
        .def("normalized", &V::normalized)
        .def("distance", &V::distance, py::arg("other"))
        .def("distance_squared", &V::distance_squared, py::arg("other"))
        .def("sum", &V::sum)
        .def("mean", &V::mean)
        .def("__repr__", [name](const V& v)
            {
                std::string text = name + "(";
                for (size_t i = 0; i < N; ++i)
                {
                    text += (i == 0 ? "" : ", ") + element_repr(v[i]);
                }
                return text + ")";
            })
        .def_buffer([](V& v) { return py::buffer_info(&v[0], sizeof(T), py::format_descriptor<T>::format(), 1, { N }, { sizeof(T) }); });

    module.def("dot", [](const V& a, const V& b) { return dot(a, b); }, py::arg("a"), py::arg("b"));
    module.def("lerp", [](const V& a, const V& b, T t) { return lerp(a, b, t); }, py::arg("a"), py::arg("b"), py::arg("t"));
    module.def("approx_equal", [](const V& a, const V& b, T epsilon) { return approx_equal(a, b, epsilon); }, py::arg("a"), py::arg("b"), py::arg("epsilon") = math::EPSILON<T>);
    if constexpr (N == 3)
    {
        module.def("cross", [](const V& a, const V& b) { return cross(a, b); }, py::arg("a"), py::arg("b"));
    }
}

// Vector types must be bound first: a matrix multiplies them.
template<size_t R, size_t C, typename T>
void bind_matrix(pybind11::module_& module)
{
    namespace py = pybind11;
    using M = Matrix<R, C, T>;

    std::string name = matrix_name<R, C, T>();
    py::class_<M> type(module, name.c_str(), py::buffer_protocol(), "A fixed-size row-major matrix; also a buffer, so numpy.asarray(m) works.");
    type.def(py::init<>())
        .def(py::init([name](const std::vector<std::vector<T>>& rows)
            {
                bool shaped = rows.size() == R;
                for (const std::vector<T>& row : rows)
                {
                    shaped = shaped && row.size() == C;
                }
                if (!shaped)
                {
                    throw Error(name + " needs " + std::to_string(R) + " rows of " + std::to_string(C) + " values");
                }
                M result;
                for (size_t row = 0; row < R; ++row)
                {
                    for (size_t col = 0; col < C; ++col)
                    {
                        result.at(row, col) = rows[row][col];
                    }
                }
                return result;
            }), py::arg("rows"))
        .def("at", [](const M& m, int64_t row, int64_t col) { return m.at(wrapped_index(row, R), wrapped_index(col, C)); }, py::arg("row"), py::arg("col"))
        .def("__getitem__", [](const M& m, std::pair<int64_t, int64_t> index) { return m.at(wrapped_index(index.first, R), wrapped_index(index.second, C)); })
        .def("__setitem__", [](M& m, std::pair<int64_t, int64_t> index, T value) { m.at(wrapped_index(index.first, R), wrapped_index(index.second, C)) = value; })
        .def("__add__", [](const M& a, const M& b) { return a + b; }, py::is_operator())
        .def("__sub__", [](const M& a, const M& b) { return a - b; }, py::is_operator())
        .def("__mul__", [](const M& m, T scalar) { return m * scalar; }, py::is_operator())
        .def("__rmul__", [](const M& m, T scalar) { return m * scalar; }, py::is_operator())
        .def("__matmul__", [](const M& m, const Vector<C, T>& v) { return m * v; }, py::is_operator())
        .def("__eq__", [](const M& a, const M& b) { return a == b; }, py::is_operator())
        .def("__ne__", [](const M& a, const M& b) { return !(a == b); }, py::is_operator())
        .def("__repr__", [name](const M& m)
            {
                std::string text = name + "([";
                for (size_t row = 0; row < R; ++row)
                {
                    text += (row == 0 ? "[" : ", [");
                    for (size_t col = 0; col < C; ++col)
                    {
                        text += (col == 0 ? "" : ", ") + element_repr(m.at(row, col));
                    }
                    text += "]";
                }
                return text + "])";
            })
        .def_buffer([](M& m) { return py::buffer_info(&m.at(0, 0), sizeof(T), py::format_descriptor<T>::format(), 2, { R, C }, { sizeof(T) * C, sizeof(T) }); });

    if constexpr (R == C)
    {
        type.def_static("identity", []() { return M::identity(); })
            .def("transpose", &M::transpose)
            .def("__matmul__", [](const M& a, const M& b) { return a * b; }, py::is_operator());
    }
    if constexpr (requires(const M& m) { m.determinant(); })
    {
        type.def("determinant", &M::determinant)
            .def("inverse", [name](const M& m)
                {
                    if (math::abs(m.determinant()) <= math::EPSILON<T>)
                    {
                        throw Error(name + " is singular and cannot be inverted");
                    }
                    return m.inverse();
                });
    }
}

} // namespace oryx::python
