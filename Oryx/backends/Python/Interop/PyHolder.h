#pragma once

#include <pybind11/pybind11.h>

PYBIND11_DECLARE_HOLDER_TYPE(T, oryx::SharedPtr<T>)
