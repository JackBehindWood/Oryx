#include "doctest.h"

#include "unit/Python/PythonTestSupport.h"

using namespace oryx;
using namespace oryx::test;

#ifdef OX_ENABLE_PYTHON

TEST_CASE("oryx.math vectors construct, index and repr like the C++ Vec3d")
{
    Vec3d expected = Vec3d(1.0, 2.0, 3.0) + Vec3d(4.0, 5.0, 6.0);

    std::string output = run_oryx_script(
        "V = oryx.math.Vec3\n"
        "a = V(1, 2, 3)\n"
        "mark(repr(a + V([4, 5, 6])) + '|' + repr(V(2)) + '|' + repr(V()) + '|')\n"
        "a.y = 9\n"
        "mark(str(a.x) + str(a[1]) + str(a[-1]) + str(len(a)) + str(list(a)) + '|')\n"
        "for call in (lambda: a[3], lambda: V([1, 2]), lambda: V(1, 2)):\n"
        "    try:\n"
        "        call()\n"
        "    except (IndexError, oryx.OryxError, TypeError) as e:\n"
        "        mark(type(e).__name__ + ';')\n");

    CHECK(output == "Vec3(" + std::to_string(expected.x()).substr(0, 1) + ".0, 7.0, 9.0)|Vec3(2.0, 2.0, 2.0)|Vec3(0.0, 0.0, 0.0)|1.09.03.03[1.0, 9.0, 3.0]|IndexError;OryxError;TypeError;");
}

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

TEST_CASE("oryx.math matrices multiply, transpose and invert")
{
    std::string output = run_oryx_script(
        "m = oryx.math\n"
        "a = m.Mat2([[1, 2], [3, 4]])\n"
        "mark(repr(a) + '|' + repr(a.transpose()) + '|' + str(a.determinant()) + '|' + repr(a @ m.Mat2.identity()) + '|' + repr(a @ m.Vec2(1, 1)) + '|')\n"
        "mark(str(a @ a.inverse() == m.Mat2.identity()) + str(a[1, 0]) + str(a.at(-1, -1)) + repr(a * 2) + repr(a + a) + '|')\n"
        "for call in (lambda: m.Mat2([[1, 2], [2, 4]]).inverse(), lambda: m.Mat2([[1, 2]]), lambda: a[2, 0]):\n"
        "    try:\n"
        "        call()\n"
        "    except (IndexError, oryx.OryxError) as e:\n"
        "        mark(type(e).__name__ + ':' + str(e) + ';')\n"
        "mark(str(hasattr(m.Mat4, 'determinant')) + str(hasattr(m.Mat3, 'inverse')))\n");

    CHECK(output ==
          "Mat2([[1.0, 2.0], [3.0, 4.0]])|Mat2([[1.0, 3.0], [2.0, 4.0]])|-2.0|Mat2([[1.0, 2.0], [3.0, 4.0]])|Vec2(3.0, 7.0)|"
          "True3.04.0Mat2([[2.0, 4.0], [6.0, 8.0]])Mat2([[2.0, 4.0], [6.0, 8.0]])|"
          "OryxError:Mat2 is singular and cannot be inverted;OryxError:Mat2 needs 2 rows of 2 values;IndexError:index out of range;"
          "FalseTrue");
}

TEST_CASE("oryx.math vectors and matrices expose the buffer protocol")
{
    std::string output = run_oryx_script(
        "m = oryx.math\n"
        "v = memoryview(m.Vec3(1, 2, 3))\n"
        "w = memoryview(m.Mat2([[1, 2], [3, 4]]))\n"
        "mark(str(v.tolist()) + str(v.shape) + str(v.format) + '|' + str(w.tolist()) + str(w.shape) + str(w.strides))\n");

    CHECK(output == "[1.0, 2.0, 3.0](3,)d|[[1.0, 2.0], [3.0, 4.0]](2, 2)(16, 8)");
}

TEST_CASE("oryx.math scalar helpers and constants")
{
    std::string output = run_oryx_script(
        "m = oryx.math\n"
        "mark(f'{m.PI:.5f}|{m.radians(180.0):.5f}|{m.degrees(m.PI):.1f}|{m.saturate(2.0)}|{m.sign(-3.0)}|{m.smoothstep(0.0, 1.0, 0.5)}|{m.approx_equal(1.0, 1.000001)}')\n");

    CHECK(output == "3.14159|3.14159|180.0|1.0|-1.0|0.5|True");
}

#endif
