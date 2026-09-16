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

} // namespace oryx
