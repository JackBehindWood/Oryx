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
}
