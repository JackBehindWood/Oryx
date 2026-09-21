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

C++ exceptions are for exceptional failures, while ordinary game operations should preferably make invalid states difficult to create.

### Error Policy

Introduced with the Phase 7 scripting work, for new code; existing error sites (such as Oasis's command-line parsing and `OX_CORE_ASSERT`) predate it and are not retrofitted.

* **Inner code throws and never catches.** Failures are reported by throwing an `oryx::Error` (`Oryx/Core/Error.h`), so library code carries no error-handling clutter.
* **`Error` is a small hierarchy.** It derives from `std::runtime_error`, carries a `detail()` (context such as a script traceback) and a virtual `category()` that each subclass overrides (so the type is the category, not a constructor string), and each module derives thin subclasses for its own failures: `ParamError` (an unknown or mistyped construction parameter, naming the key) `SettingsError` (an unreadable or malformed settings file, or a wrong value) and `AssertionError` (a failed check) in Core, `ScriptError` (carrying the script's traceback) in `Scripting/`. Code that cares catches the typed error; boundaries catch `Error`.
* **A few boundaries catch, and they are layers.** `LayerStack` calls a layer's `attach()`, `update()`, `event()` and `detach()` through one guarded call. A layer that throws is logged once and then disabled: it is skipped for `update()` and `event()`, still `detach()`ed on shutdown, and the other layers keep running. The `Application` learns of it through `on_layer_disabled()` and may react (Oasis closes with a non-zero exit code). A layer may also catch around a unit of independent work, as `ScriptingLayer` does per script so one broken script does not hide the others, and around a settings reload so a bad edit keeps the previous settings. The other catch sites are the process boundary in `EntryPoint.h` (the initial settings load) and `oryx::shutdown()`, which isolates each shutdown hook.
* **Logging is explicit.** `Error::log()` writes `[category] message` and the detail through the core logger. Nothing logs when an error is constructed or thrown, so a rejected user input is never logged twice, and only the catching boundary decides where the report goes.
* **Logging is always initialised.** The host calls `oryx::init()` before any layer runs (`EntryPoint.h`'s `main()`, the test `main()`, and `oryx.init()` from a REPL or notebook); it is idempotent and `oryx::is_initialised()` reports the state. There is no standard-error fallback, so code that logs may assume the loggers exist.
* **Assertions have one mechanism.** `OX_ASSERT`/`OX_CORE_ASSERT` (Debug builds only) call the replaceable `AssertionHandler` from `Core/Assert.h`: the default logs and breaks into the debugger, `throw_on_assertion` throws an `AssertionError`. `check(condition, message)` is always compiled and always throws `AssertionError` (never logs, so the boundary reports it once), independent of the handler; use it where a failure must be catchable in every profile.
* **Lookup misses are not errors.** `Registry<T>::create()` still returns `nullptr` for an unknown name, as before; only invalid arguments throw.
