#include "doctest.h"

#include "Oryx.h"

TEST_CASE("Matrix::identity produces a square identity matrix")
{
    auto identity = oryx::Matrix<3, 3, float>::identity();

    for (size_t row = 0; row < 3; ++row)
    {
        for (size_t col = 0; col < 3; ++col)
        {
            CHECK(identity.at(row, col) == doctest::Approx(row == col ? 1.0f : 0.0f));
        }
    }
}

TEST_CASE("Matrix operator* multiplies against a known result")
{
    oryx::Matrix<2, 2, int> a;
    a.at(0, 0) = 1; a.at(0, 1) = 2;
    a.at(1, 0) = 3; a.at(1, 1) = 4;

    auto identity = oryx::Matrix<2, 2, int>::identity();
    auto result = a * identity;

    CHECK(result.at(0, 0) == 1);
    CHECK(result.at(0, 1) == 2);
    CHECK(result.at(1, 0) == 3);
    CHECK(result.at(1, 1) == 4);
}

TEST_CASE("Matrix transpose round-trips")
{
    oryx::Matrix<2, 3, int> m;
    m.at(0, 0) = 1; m.at(0, 1) = 2; m.at(0, 2) = 3;
    m.at(1, 0) = 4; m.at(1, 1) = 5; m.at(1, 2) = 6;

    auto transposed = oryx::transpose(m);
    CHECK(transposed.at(0, 0) == 1);
    CHECK(transposed.at(1, 0) == 2);
    CHECK(transposed.at(2, 0) == 3);
    CHECK(transposed.at(0, 1) == 4);
    CHECK(transposed.at(1, 1) == 5);
    CHECK(transposed.at(2, 1) == 6);

    auto round_tripped = oryx::transpose(transposed);
    for (size_t row = 0; row < 2; ++row)
    {
        for (size_t col = 0; col < 3; ++col)
        {
            CHECK(round_tripped.at(row, col) == m.at(row, col));
        }
    }

    // Member transpose() delegates to the free function.
    auto member_transposed = m.transpose();
    for (size_t row = 0; row < 3; ++row)
    {
        for (size_t col = 0; col < 2; ++col)
        {
            CHECK(member_transposed.at(row, col) == transposed.at(row, col));
        }
    }
}

TEST_CASE("Mat2, Mat3 and Mat4 aliases instantiate with expected underlying types")
{
    oryx::Mat2f m2f;
    oryx::Mat2d m2d;
    oryx::Mat3f m3f;
    oryx::Mat3d m3d;
    oryx::Mat4f m4f;
    oryx::Mat4d m4d;

    CHECK(sizeof(m2f.at(0, 0)) == sizeof(float));
    CHECK(sizeof(m2d.at(0, 0)) == sizeof(double));
    CHECK(sizeof(m3f.at(0, 0)) == sizeof(float));
    CHECK(sizeof(m3d.at(0, 0)) == sizeof(double));
    CHECK(sizeof(m4f.at(0, 0)) == sizeof(float));
    CHECK(sizeof(m4d.at(0, 0)) == sizeof(double));
}

TEST_CASE("Matrix operator+ and operator- are componentwise")
{
    oryx::Matrix<2, 2, int> a;
    a.at(0, 0) = 1; a.at(0, 1) = 2; a.at(1, 0) = 3; a.at(1, 1) = 4;

    oryx::Matrix<2, 2, int> b;
    b.at(0, 0) = 5; b.at(0, 1) = 6; b.at(1, 0) = 7; b.at(1, 1) = 8;

    auto sum = a + b;
    CHECK(sum.at(0, 0) == 6);
    CHECK(sum.at(0, 1) == 8);
    CHECK(sum.at(1, 0) == 10);
    CHECK(sum.at(1, 1) == 12);

    auto diff = b - a;
    CHECK(diff.at(0, 0) == 4);
    CHECK(diff.at(0, 1) == 4);
    CHECK(diff.at(1, 0) == 4);
    CHECK(diff.at(1, 1) == 4);
}

TEST_CASE("Matrix scalar multiply scales every element")
{
    oryx::Matrix<2, 2, int> a;
    a.at(0, 0) = 1; a.at(0, 1) = 2; a.at(1, 0) = 3; a.at(1, 1) = 4;

    auto result = a * 2;
    CHECK(result.at(0, 0) == 2);
    CHECK(result.at(0, 1) == 4);
    CHECK(result.at(1, 0) == 6);
    CHECK(result.at(1, 1) == 8);
}

TEST_CASE("Matrix operator== compares elementwise")
{
    auto a = oryx::Matrix<2, 2, int>::identity();
    auto b = oryx::Matrix<2, 2, int>::identity();
    oryx::Matrix<2, 2, int> c;

    CHECK(a == b);
    CHECK_FALSE(a == c);
}

TEST_CASE("Matrix-vector multiply transforms a vector by a known matrix")
{
    auto identity = oryx::Matrix<2, 2, float>::identity();
    oryx::Vec2f v(3.0f, 4.0f);
    CHECK((identity * v) == v);

    oryx::Matrix<2, 2, float> m;
    m.at(0, 0) = 2.0f; m.at(0, 1) = 0.0f;
    m.at(1, 0) = 0.0f; m.at(1, 1) = 3.0f;
    auto result = m * v;
    CHECK(result.x() == doctest::Approx(6.0f));
    CHECK(result.y() == doctest::Approx(12.0f));
}

TEST_CASE("Matrix::determinant matches known values for 2x2 and 3x3")
{
    oryx::Matrix<2, 2, float> m2;
    m2.at(0, 0) = 3.0f; m2.at(0, 1) = 8.0f;
    m2.at(1, 0) = 4.0f; m2.at(1, 1) = 6.0f;
    CHECK(oryx::determinant(m2) == doctest::Approx(-14.0f));
    CHECK(m2.determinant() == doctest::Approx(-14.0f));

    auto identity3 = oryx::Matrix<3, 3, float>::identity();
    CHECK(oryx::determinant(identity3) == doctest::Approx(1.0f));
}

TEST_CASE("Matrix::inverse round-trips: m * m.inverse() is approximately identity")
{
    oryx::Matrix<2, 2, float> m;
    m.at(0, 0) = 4.0f; m.at(0, 1) = 7.0f;
    m.at(1, 0) = 2.0f; m.at(1, 1) = 6.0f;

    auto inv = m.inverse();
    auto result = m * inv;
    auto identity = oryx::Matrix<2, 2, float>::identity();
    for (size_t row = 0; row < 2; ++row)
    {
        for (size_t col = 0; col < 2; ++col)
        {
            CHECK(result.at(row, col) == doctest::Approx(identity.at(row, col)));
        }
    }

    auto identity3 = oryx::Matrix<3, 3, float>::identity();
    auto inv3 = oryx::inverse(identity3);
    for (size_t row = 0; row < 3; ++row)
    {
        for (size_t col = 0; col < 3; ++col)
        {
            CHECK(inv3.at(row, col) == doctest::Approx(identity3.at(row, col)));
        }
    }
}

TEST_CASE("Matrix::determinant is approximately zero for a known-singular matrix")
{
    // Deliberately only exercised through determinant(), never inverse() — calling
    // inverse() on an actually-singular matrix trips OX_CORE_ASSERT's SIGTRAP in the
    // Debug configuration the Tests binary is built in, which would abort the whole
    // binary rather than fail a single CHECK.
    oryx::Matrix<2, 2, float> singular;
    singular.at(0, 0) = 1.0f; singular.at(0, 1) = 2.0f;
    singular.at(1, 0) = 2.0f; singular.at(1, 1) = 4.0f;

    CHECK(oryx::determinant(singular) == doctest::Approx(0.0f));
}

TEST_CASE("translation, rotation and scale build the expected Matrix3, and transform_point applies them")
{
    auto t = oryx::translation(oryx::Vec2f(5.0f, -2.0f));
    oryx::Vec2f p(1.0f, 1.0f);
    auto translated = oryx::transform_point(t, p);
    CHECK(translated.x() == doctest::Approx(6.0f));
    CHECK(translated.y() == doctest::Approx(-1.0f));

    auto s = oryx::scale(oryx::Vec2f(2.0f, 3.0f));
    auto scaled = oryx::transform_point(s, p);
    CHECK(scaled.x() == doctest::Approx(2.0f));
    CHECK(scaled.y() == doctest::Approx(3.0f));

    auto r = oryx::rotation(oryx::math::HALF_PI<float>);
    oryx::Vec2f right(1.0f, 0.0f);
    auto rotated = oryx::transform_point(r, right);
    CHECK(rotated.x() == doctest::Approx(0.0f).epsilon(0.001));
    CHECK(rotated.y() == doctest::Approx(1.0f).epsilon(0.001));
}

TEST_CASE("Non-square Matrix multiply still works")
{
    oryx::Matrix<2, 3, int> a;
    a.at(0, 0) = 1; a.at(0, 1) = 2; a.at(0, 2) = 3;
    a.at(1, 0) = 4; a.at(1, 1) = 5; a.at(1, 2) = 6;

    oryx::Matrix<3, 2, int> b;
    b.at(0, 0) = 7; b.at(0, 1) = 8;
    b.at(1, 0) = 9; b.at(1, 1) = 10;
    b.at(2, 0) = 11; b.at(2, 1) = 12;

    auto result = a * b;
    CHECK(result.at(0, 0) == 58);
    CHECK(result.at(0, 1) == 64);
    CHECK(result.at(1, 0) == 139);
    CHECK(result.at(1, 1) == 154);
}

TEST_CASE("to_string formats a Matrix's rows")
{
    auto m = oryx::Matrix<2, 2, int>::identity();
    CHECK(oryx::to_string(m) == "[1, 0]\n[0, 1]");
}
