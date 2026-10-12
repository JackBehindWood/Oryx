from pathlib import Path

from pyforge.api import BoundarySpec, BoundarySuite


def oryx_boundaries(project_root: Path) -> BoundarySuite:
    return BoundarySuite(engine_boundaries(project_root), oasis_boundaries())


def engine_boundaries(project_root: Path) -> BoundarySpec:
    spec = BoundarySpec(name="oryx", root="Oryx/src", base="Oryx", namespace="oryx")
    t = spec.discover(project_root)

    core, game, strategy, simulation, text, board = t.Core, t.Game, t.Strategy, t.Simulation, t.Text, t.Board
    assets, dashboard, shaders, renderer = t.Assets, t.Dashboard, t.Shaders, t.Renderer
    canvas, ui, gui = t.Interface.Canvas, t.Interface.UI, t.Interface.GUI
    feed, view, panel = t.Dashboard.Feed, t.Dashboard.View, t.Dashboard.Panel
    board_graphics = t.Board.Graphics

    graphics = spec.group("graphics", t.Graphics, shaders, renderer, canvas, ui, gui, view, panel, board_graphics)
    headless = spec.group("headless", game, strategy, simulation, core, text, board, feed)

    backends = spec.files("backends", "**/backends/**", allow_empty=True)
    backend_headers = spec.files("backend-headers", "{Null,Metal,MacOS,Glfw}*.{h,hpp}")
    backend_factories = spec.files("backend-factories", "Core/Window.cpp", "Graphics/RHI/RHI.cpp")
    window_headers = spec.files("window-headers", "Core/Window.h", "Core/Input.h", "Events/WindowEvent.h")
    gpu_bridges = spec.files("gpu-bridges", "Assets/**/Gpu*")

    backends.must_not_be_included(because="public headers never include backend code")
    backend_headers.only_included_by(backend_factories, because="only the backend factories pick a backend")

    assets.must_not_include(window_headers, unless=gpu_bridges, because="CPU assets stay headless")
    assets.must_not_include(graphics, unless=gpu_bridges, because="only the Gpu* bridges reach graphics")
    spec.forbid(headless, graphics, unless=board_graphics, because="only Board/Graphics sits above the renderer")
    spec.forbid([graphics, text], assets, because="fonts reach the renderer through Text/IFontSource")

    canvas.must_not_include(ui, gui, board, assets, because="the shared immediate-mode layer")
    spec.peers(ui, gui, because="the player UI and developer tooling never mix")
    ui.must_not_include(board, assets, game, strategy, because="the player-facing layer")
    gui.must_not_include(board, assets, game, strategy, dashboard, because="the developer-tooling layer")
    spec.isolate([feed, view, panel], from_=[board, assets], because="the dashboard reads only feed data")
    spec.above(dashboard, core, game, strategy, simulation, because="these sit below the dashboard")

    spec.layers(t.Graphics, shaders, renderer, canvas, ui, board_graphics)
    spec.layers(renderer, canvas, gui, view, panel)

    python = spec.symbols("python", "oryx::python", "oryx::PythonRuntime", "pybind11")
    cpython = spec.symbols("cpython", prefixes=["Py", "_Py"])

    optional_graphics = spec.optional("graphics", graphics, because="graphics is optional and can be disabled at build time")

    lib = spec.library("build/bin/**/libOryx.a", when_off=optional_graphics)
    lib.must_not_export(graphics, because="a graphics-off build has no graphics code")
    lib.must_not_import("*glfw*", "*MTL*", because="a graphics-off build has no window or GPU dependency")

    optional_python = spec.optional("python", python, because="the Python scripting backend is optional and can be disabled at build time")
    lib.must_not_export(python, because="a Python-off build has no scripting backend", when_off=optional_python)
    lib.must_not_import(cpython, because="a Python-off build has no CPython dependency", when_off=optional_python)
    return spec


def oasis_boundaries() -> BoundarySpec:
    spec = BoundarySpec(name="oasis", root="Oasis/src", base="Oasis")
    core, game, strategy = spec.modules("Core", "Game", "Strategy")
    oryx_graphics = spec.group("oryx-graphics", *spec.modules("Oryx/Graphics", "Oryx/Shaders", "Oryx/Renderer", "Oryx/Interface", allow_empty=True))

    spec.above(core, game, strategy, because="the app shell sits on top of the rules")
    game.must_not_include(strategy, because="game rules do not depend on strategies")
    spec.isolate([game, strategy], from_=oryx_graphics, because="game rules and strategies stay headless")
    return spec
