#include "doctest.h"

#include "unit/Python/PythonTestSupport.h"

using namespace oryx;
using namespace oryx::test;

#ifdef OX_ENABLE_PYTHON

TEST_CASE("oryx.math vector operations agree with the C++ functions")
{
    Vec3d a(1.0, 2.0, 3.0);
    Vec3d b(4.0, 5.0, 6.0);
    std::string expected = std::to_string(dot(a, b)) + "|" + std::to_string(length(a)) + "|" + std::to_string(distance(a, b)) + "|" + std::to_string(cross(a, b)[2]);

    std::string output = run_oryx_script(
        "m = oryx.math\n"
        "a = m.Vec3(1, 2, 3)\n"
        "b = m.Vec3(4, 5, 6)\n"
        "mark(f'{m.dot(a, b):.6f}|{a.length():.6f}|{a.distance(b):.6f}|{m.cross(a, b).z:.6f}')\n"
        "mark(';' + repr(a * 2) + repr(2 * a) + repr(b - a) + repr(-a) + repr(b / 2) + repr(a * b))\n"
        "mark(';' + str(a == m.Vec3(1, 2, 3)) + str(a != b) + str(m.approx_equal(a, m.Vec3(1, 2, 3.000001))))\n"
        "mark(';' + repr(m.lerp(m.Vec2(0, 0), m.Vec2(2, 4), 0.5)) + str(m.lerp(0.0, 10.0, 0.25)) + str(m.clamp(5.0, 0.0, 1.0)))\n");

    CHECK(output ==
          expected + ";Vec3(2.0, 4.0, 6.0)Vec3(2.0, 4.0, 6.0)Vec3(3.0, 3.0, 3.0)Vec3(-1.0, -2.0, -3.0)Vec3(2.0, 2.5, 3.0)Vec3(4.0, 10.0, 18.0)"
          ";TrueTrueTrue;Vec2(1.0, 2.0)2.51.0");
}

#endif
