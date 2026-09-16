#pragma once

namespace oryx
{

template<size_t R, size_t C, typename T>
class Matrix
{
public:
    Matrix() = default;

    static Matrix<R, C, T> identity()
    {
        static_assert(R == C, "Matrix::identity() requires a square matrix");
        Matrix<R, C, T> result;
        for (size_t i = 0; i < R; ++i)
        {
            result.at(i, i) = T{ 1 };
        }
        return result;
    }

    T& at(size_t row, size_t col) { return m_data[row * C + col]; }
    const T& at(size_t row, size_t col) const { return m_data[row * C + col]; }

private:
    T m_data[R * C]{};
};

template<size_t R, size_t K, size_t C, typename T>
Matrix<R, C, T> operator*(const Matrix<R, K, T>& a, const Matrix<K, C, T>& b)
{
    Matrix<R, C, T> result;
    for (size_t row = 0; row < R; ++row)
    {
        for (size_t col = 0; col < C; ++col)
        {
            T sum{};
            for (size_t k = 0; k < K; ++k)
            {
                sum += a.at(row, k) * b.at(k, col);
            }
            result.at(row, col) = sum;
        }
    }
    return result;
}

template<size_t R, size_t C, typename T>
Matrix<C, R, T> transpose(const Matrix<R, C, T>& m)
{
    Matrix<C, R, T> result;
    for (size_t row = 0; row < R; ++row)
    {
        for (size_t col = 0; col < C; ++col)
        {
            result.at(col, row) = m.at(row, col);
        }
    }
    return result;
}

} // namespace oryx
