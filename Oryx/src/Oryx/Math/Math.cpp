#include "oxpch.h"
#include "Oryx/Math/Math.h"

namespace oryx
{

// Explicit instantiation: compiled once here instead of per-TU.
template class Vector<2, float>;
template class Vector<2, double>;
template class Vector<2, int>;
template class Vector<3, float>;
template class Vector<3, double>;
template class Vector<3, int>;
template class Vector<4, float>;
template class Vector<4, double>;
template class Vector<4, int>;

// Only the named/aliased square shapes are explicitly instantiated — arbitrary
// R x C usage (e.g. Matrix<2,3,T>) still instantiates per-TU as before.
template class Matrix<2, 2, float>;
template class Matrix<2, 2, double>;
template class Matrix<3, 3, float>;
template class Matrix<3, 3, double>;
template class Matrix<4, 4, float>;
template class Matrix<4, 4, double>;

} // namespace oryx
