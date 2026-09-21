#include "oxpch.h"
#include "Bindings/BindOryx.h"
#include "Bindings/BindMath.h"

namespace py = pybind11;

namespace oryx::python
{

namespace
{

// Adding a vector or matrix type is one line in these tables; the Python name, operators and buffer come from the templates in BindMath.h.
constexpr std::array<void (*)(py::module_&), 3> kVectors = {
    &bind_vector<2, double>, &bind_vector<3, double>, &bind_vector<4, double>,
};

constexpr std::array<void (*)(py::module_&), 3> kMatrices = {
    &bind_matrix<2, 2, double>, &bind_matrix<3, 3, double>, &bind_matrix<4, 4, double>,
};

void bind_scalar_functions(py::module_& module)
{
    module.attr("PI") = math::PI<double>;
    module.attr("TWO_PI") = math::TWO_PI<double>;
    module.attr("HALF_PI") = math::HALF_PI<double>;
    module.attr("EPSILON") = math::EPSILON<double>;
    module.def("clamp", &math::clamp<double>, py::arg("value"), py::arg("low"), py::arg("high"));
    module.def("lerp", &math::lerp<double>, py::arg("a"), py::arg("b"), py::arg("t"));
    module.def("saturate", &math::saturate<double>, py::arg("value"));
    module.def("sign", &math::sign<double>, py::arg("value"));
    module.def("smoothstep", &math::smoothstep<double>, py::arg("edge0"), py::arg("edge1"), py::arg("x"));
    module.def("radians", &math::radians<double>, py::arg("degrees"));
    module.def("degrees", &math::degrees<double>, py::arg("radians"));
    module.def("approx_equal", &math::approx_equal<double>, py::arg("a"), py::arg("b"), py::arg("epsilon") = math::EPSILON<double>);
}

// Constants and functions that belong to no type: one line each in bind_scalar_functions.
constexpr std::array<void (*)(py::module_&), 1> kOneOffs = {
    &bind_scalar_functions,
};

} // namespace

void bind_math(py::module_& module)
{
    py::module_ math_module = module.def_submodule("math", "Vectors and matrices of doubles, and scalar helpers.");
    for (void (*binder)(py::module_&) : kVectors)
    {
        binder(math_module);
    }
    for (void (*binder)(py::module_&) : kMatrices)
    {
        binder(math_module);
    }
    for (void (*binder)(py::module_&) : kOneOffs)
    {
        binder(math_module);
    }
}

} // namespace oryx::python
