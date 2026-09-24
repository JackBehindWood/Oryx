import oryx
import pytest


def test_vectors_construct_index_and_repr():
    m = oryx.math
    V = m.Vec3
    a = V(1, 2, 3)
    assert repr(a + V([4, 5, 6])) == "Vec3(5.0, 7.0, 9.0)"
    assert repr(V(2)) == "Vec3(2.0, 2.0, 2.0)"
    assert repr(V()) == "Vec3(0.0, 0.0, 0.0)"

    a.y = 9
    assert (a.x, a[1], a[-1], len(a), list(a)) == (1.0, 9.0, 3.0, 3, [1.0, 9.0, 3.0])

    with pytest.raises(IndexError):
        a[3]
    with pytest.raises(oryx.OryxError):
        V([1, 2])
    with pytest.raises(TypeError):
        V(1, 2)


def test_matrices_multiply_transpose_and_invert():
    m = oryx.math
    a = m.Mat2([[1, 2], [3, 4]])
    assert repr(a) == "Mat2([[1.0, 2.0], [3.0, 4.0]])"
    assert repr(a.transpose()) == "Mat2([[1.0, 3.0], [2.0, 4.0]])"
    assert a.determinant() == -2.0
    assert repr(a @ m.Mat2.identity()) == repr(a)
    assert repr(a @ m.Vec2(1, 1)) == "Vec2(3.0, 7.0)"

    assert a @ a.inverse() == m.Mat2.identity()
    assert a[1, 0] == 3.0
    assert a.at(-1, -1) == 4.0
    assert repr(a * 2) == "Mat2([[2.0, 4.0], [6.0, 8.0]])"
    assert repr(a + a) == "Mat2([[2.0, 4.0], [6.0, 8.0]])"

    with pytest.raises(oryx.OryxError, match="Mat2 is singular and cannot be inverted"):
        m.Mat2([[1, 2], [2, 4]]).inverse()
    with pytest.raises(oryx.OryxError, match="Mat2 needs 2 rows of 2 values"):
        m.Mat2([[1, 2]])
    with pytest.raises(IndexError, match="index out of range"):
        a[2, 0]

    assert hasattr(m.Mat4, "determinant") is False
    assert hasattr(m.Mat3, "inverse") is True


def test_vectors_and_matrices_expose_the_buffer_protocol():
    m = oryx.math
    v = memoryview(m.Vec3(1, 2, 3))
    w = memoryview(m.Mat2([[1, 2], [3, 4]]))
    assert v.tolist() == [1.0, 2.0, 3.0]
    assert v.shape == (3,)
    assert v.format == "d"
    assert w.tolist() == [[1.0, 2.0], [3.0, 4.0]]
    assert w.shape == (2, 2)
    assert w.strides == (16, 8)


def test_scalar_helpers_and_constants():
    m = oryx.math
    assert f"{m.PI:.5f}" == "3.14159"
    assert f"{m.radians(180.0):.5f}" == "3.14159"
    assert f"{m.degrees(m.PI):.1f}" == "180.0"
    assert m.saturate(2.0) == 1.0
    assert m.sign(-3.0) == -1.0
    assert m.smoothstep(0.0, 1.0, 0.5) == 0.5
    assert m.approx_equal(1.0, 1.000001) is True
