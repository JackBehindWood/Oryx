#include "oxpch.h"
#include "BindOryx.h"

#include "Oryx/Core/Random.h"

namespace py = pybind11;

namespace oryx::python
{

void bind_random(py::module_& module)
{
    py::class_<Random>(module, "Random", "Uniform random number generator; seeded from the clock unless given a seed.")
        .def(py::init<>())
        .def(py::init<uint64_t>(), py::arg("seed"))
        .def("seed", &Random::seed, py::arg("seed"))
        .def("get_int", &Random::get_int, py::arg("min") = 0, py::arg("max") = std::numeric_limits<int64_t>::max())
        .def("get_double", &Random::get_double, py::arg("min") = 0.0, py::arg("max") = 1.0)
        .def("get_bool", &Random::get_bool, py::arg("p") = 0.5);
}

} // namespace oryx::python
