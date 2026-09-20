# C++ API

## C++ API Design

C++ is the core implementation language.

The C++ API should prioritise:

* Clear ownership
* Explicit lifetimes
* Value semantics where practical
* Minimal hidden allocation
* Const-correctness
* Testability
* Predictable performance
* Readable modern C++

Templates should be used where they provide meaningful benefits, not merely because they are available.

Likewise, advanced metaprogramming should not become a prerequisite for understanding the engine.

### Naming Conventions

* Functions and methods: `snake_case` (e.g. `legal_actions()`)
* Classes: `PascalCase` (e.g. `class Rewards`)
* Interfaces (no data members; every method pure virtual, apart from optional
  capability hooks that default to "not provided" — `IGame::action_features()`
  returns `nullptr`, `IStrategy::required_capabilities()` returns `{}`)
  additionally get an `I`-prefix (e.g. `IGame`, `IState`, `IStrategy`).
  Base classes that mix concrete behaviour with virtual methods do not —
  e.g. `Application` keeps its name (it has real state — `m_running`, the
  `LayerStack` — and concrete methods like `run()`/`close()`/`push_layer()`);
  `Layer` (`Oryx/Core/Layer.h`) is the same case, with a concrete `name()`
  alongside `attach()`/`detach()`/`update()`/`event()` virtuals that default
  to no-ops rather than being pure.
* Structs are data-only: plain fields, no member functions. Any behaviour
  needed on struct-held data is a free function instead (e.g. `Outcome`
  has `is_terminal`/`rewards` fields only; `Colour` has `r`/`g`/`b`/`a`
  fields only, with `operator==`, `approx_equal` and `lerp` as free
  functions).
* Exception — math types (`Vector<N, T>`, `Matrix<R, C, T>`): these are
  `class`, not data-only structs, and deliberately expose *both* member
  functions and equivalent free functions for the operations where a
  primary receiver makes sense. For `Vector`: `length()`, `normalized()`,
  `sum()`, `mean()`, `distance()`, `distance_squared()` as members, with
  `dot`, `length`, `normalize`, `sum`, `mean`, `cross`, `distance`,
  `distance_squared`, `lerp`, `clamp`, `min`, `max`, `abs`, `approx_equal`,
  `to_string` as free functions (operators and `dot`/`cross` stay
  free-function-only; `distance`/`distance_squared` are a deliberate
  exception to that, noted inline in `Vector.h`). For `Matrix`:
  `transpose()`, `determinant()`, `inverse()` (the latter two bounded to
  2×2/3×3) as members, with the same plus the arithmetic operators as free
  functions. This mirrors the dual method/module-function API convention
  used by numpy and similar math libraries (GLM, Eigen), and is a
  documented, intentional carve-out scoped to `Vector`/`Matrix` — it does
  not loosen the data-only-struct rule elsewhere (`Colour` deliberately
  stays a plain data-only struct, not part of this exception).

This was not written down before the Phase 2 brainstorm; existing code
predates it and is not being retrofitted (e.g. `Application::Get()` is a
static accessor, `create_application()` a free function — both fine as
historical exceptions, not examples to copy for new pure interfaces).

### Math Module

A header-only `oryx::Math` module (`Oryx/src/Oryx/Math/`), widened during
Phase 2 planning from the originally-scoped "`Vec2` struct", and widened
again for a general vector/matrix/colour pass (see [Architecture §3.4](../architecture.md#34-math)
for the full breakdown). It provides `oryx::math` (templated `<cmath>`
wrappers, plus named constants `PI`/`TWO_PI`/`HALF_PI`/`EPSILON` and
`sign`/`saturate`/`smoothstep`/`radians`/`degrees`/`approx_equal`), a
generic `Vector<N, T>` with dimension `N` as a non-type template parameter
(backed by a plain C array, not `std::array`), dedicated
`Vector2.h`/`Vector3.h`/`Vector4.h` headers for the `Vec2`/`Vec3`/`Vec4`
aliases (2D/3D also get `cross()`; 2D also gets `manhattan_distance`/
`chebyshev_distance`), a `Matrix<R, C, T>` with matrix×matrix and
matrix×vector multiply, `+`/`-`/scalar `*`/`==`, `transpose()`, and a
bounded 2×2/3×3-only `determinant()`/`inverse()` (never a general N×N
algorithm), `Matrix3.h`'s `translation`/`rotation`/`scale`/
`transform_point` 2D affine-transform helpers (via homogeneous
coordinates, no `Transform` class), and a minimal `Colour` struct:

```cpp
template<size_t N, typename T>
class Vector
{
public:
    T& operator[](size_t i);
    T length() const;
    Vector<N, T> normalized() const;
    // ...
private:
    T m_data[N]{};
};

using Vec2f = Vector<2, float>;
using Vec3f = Vector<3, float>;

struct Colour
{
    float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
};
```

`Vector<N,T>`/`Matrix<R,C,T>` are the documented exception to the
struct-is-data-only rule above; `Colour` deliberately is **not** — it stays
a plain data-only struct, with `operator==`/`approx_equal`/`lerp` as free
functions, since its behaviour is small enough that extending the
Vector/Matrix exception to a third type isn't warranted. The module is
still scoped to what board/grid games and, later, graphics actually need —
not a general-purpose maths library. The 2×2/3×3 `determinant`/`inverse`
are a deliberate, bounded addition (special-cased free-function overloads,
not a general algorithm) to unblock real 2D transform math; general N×N
determinant/inverse, 4×4 inverse, quaternions, a `Matrix4`-based 3D
transform pipeline, and a byte-based/named-colour palette remain out of
scope and should grow only from demonstrated requirements ([Design Review Principle](principles.md#design-review-principle)).

## Error Handling

The project should favour errors that are:

* Explicit
* Actionable
* Easy to diagnose
* Testable

C++ exceptions may be appropriate for exceptional failures, while ordinary game operations should preferably make invalid states difficult to create.

The precise error-handling policy should be established before public APIs become stable.
