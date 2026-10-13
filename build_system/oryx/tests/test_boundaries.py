from pathlib import Path

from pyforge.boundaries import Boundaries

from build_system.oryx.boundaries import oryx_boundaries

REPO = Path(__file__).resolve().parents[3]
SUITE = oryx_boundaries(REPO)
BOUNDARIES = Boundaries(SUITE.specs["oryx"])
OASIS = Boundaries(SUITE.specs["oasis"])


def check_includes(root: Path) -> list:
    return BOUNDARIES.check(root)


def _tree(tmp_path: Path, files: dict[str, str]) -> Path:
    for relative, content in files.items():
        path = tmp_path / "Oryx" / "src" / "Oryx" / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
    return tmp_path


def test_real_tree_is_clean():
    assert check_includes(REPO) == []


def test_headless_module_including_graphics_is_flagged(tmp_path):
    src = _tree(tmp_path, {"Game/G.h": '#include "Oryx/Renderer/Renderer.h"\n'})
    assert len(check_includes(src)) == 1


def test_upward_layer_include_is_flagged(tmp_path):
    src = _tree(tmp_path, {"Graphics/RHI/A.h": '#include "Oryx/Shaders/Shaders.h"\n', "Renderer/R.h": '#include "Oryx/Shaders/Shaders.h"\n'})
    assert len(check_includes(src)) == 1


def test_only_gpu_asset_cache_may_reach_graphics(tmp_path):
    src = _tree(tmp_path, {
        "Assets/GpuAssetCache.h": '#include "Oryx/Graphics/RHI/RHI.h"\n',
        "Assets/ImageAsset.h": '#include "Oryx/Graphics/RHI/RHI.h"\n',
    })
    assert len(check_includes(src)) == 1


def test_gpu_shader_bridges_may_reach_shaders_but_shaders_may_not_reach_assets(tmp_path):
    src = _tree(tmp_path, {
        "Assets/GpuShaderStore.h": '#include "Oryx/Shaders/Cache/ShaderBinaryStore.h"\n',
        "Assets/ShaderAsset.h": '#include "Oryx/Shaders/Cache/ShaderBinaryStore.h"\n',
        "Shaders/ShaderCache.h": '#include "Oryx/Assets/GpuShaderStore.h"\n',
    })
    assert len(check_includes(src)) == 2


def test_public_header_including_backend_is_flagged(tmp_path):
    src = _tree(tmp_path, {"Core/X.h": '#include "backends/Null/NullRHI.h"\n'})
    assert len(check_includes(src)) == 1


def test_cpu_assets_may_not_include_window_headers(tmp_path):
    src = _tree(tmp_path, {"Assets/ImageAsset.h": '#include "Oryx/Core/Window.h"\n', "Assets/GpuAssetCache.h": '#include "Oryx/Core/Window.h"\n'})
    assert len(check_includes(src)) == 1


def test_core_window_and_input_headers_stay_graphics_free(tmp_path):
    src = _tree(tmp_path, {"Core/Window.h": '#include "Oryx/Renderer/GraphicsLayer.h"\n', "Core/Input.h": '#include "Oryx/Graphics/RHI/RHI.h"\n'})
    assert len(check_includes(src)) == 2


def test_real_core_window_and_input_include_no_graphics_module():
    guarded = {"Core/Window.h", "Core/Input.h", "Core/PolledInput.h"}
    graphics = {"Graphics", "Shaders", "Renderer"}
    assert not [i for i in BOUNDARIES.includes(REPO) if i.file in guarded and i.target in graphics]


def test_backend_factories_may_include_backend_headers_but_nothing_else(tmp_path):
    src = _tree(tmp_path, {
        "Graphics/RHI/RHI.cpp": '#include "NullRHI.h"\n#include "MetalRHI.h"\n',
        "Core/Window.cpp": '#include "NullWindow.h"\n',
        "Graphics/RHI/Other.cpp": '#include "NullRHI.h"\n',
        "Renderer/R.cpp": '#include "MetalRHI.h"\n',
    })
    assert len(check_includes(src)) == 2


def test_renderer_and_text_may_not_include_assets(tmp_path):
    src = _tree(tmp_path, {
        "Renderer/Font.h": '#include "Oryx/Assets/AssetManager.h"\n',
        "Graphics/G.h": '#include "Oryx/Assets/AssetHandle.h"\n',
        "Text/T.h": '#include "Oryx/Assets/AssetHandle.h"\n',
        "Text/Ok.h": '#include "Oryx/Core/Utf8.h"\n',
        "Assets/AssetFontSource.h": '#include "Oryx/Text/IFontSource.h"\n',
    })
    assert len(check_includes(src)) == 3


def test_text_is_headless(tmp_path):
    src = _tree(tmp_path, {"Text/T.h": '#include "Oryx/Renderer/Font.h"\n'})
    assert len(check_includes(src)) == 1


def test_every_declared_module_exists_in_the_tree():
    assert BOUNDARIES.dead_parts(REPO) == []


def test_graphics_exports_are_derived_from_the_source_not_listed_by_hand():
    from pyforge.boundaries import compile_selector

    part = BOUNDARIES.spec._find("graphics")
    assert part is not None
    graphics = compile_selector(list(part.words), {})
    derived = BOUNDARIES.scan(REPO).exclusive_to(graphics)
    assert {"DrawList", "Painter", "PresentedGraphicsBoard2D", "ImContext", "UiContext", "GuiContext", "LayoutTree", "Renderer"} <= derived
    assert not {"oryx", "std", "detail"} & derived


def test_library_checks_are_tied_to_the_graphics_and_python_options():
    (library,) = BOUNDARIES.spec.libraries
    by_option = {}
    for rule in library.rules:
        by_option.setdefault(rule.requires, set()).add(rule.kind)
    assert by_option == {("!graphics",): {"export", "import"}, ("!python",): {"symbols", "import"}}
    assert {optional.option for optional in BOUNDARIES.spec.optionals} == {"graphics", "python"}


def test_oasis_tree_satisfies_its_own_rules():
    assert OASIS.check(REPO) == [] and OASIS.dead_parts(REPO) == []


def test_oasis_game_rules_cannot_reach_the_app_shell_or_the_renderer(tmp_path):
    for relative, content in {
        "Oasis/src/Oasis/Game/G.h": '#include "Oasis/Core/OasisApp.h"\n#include "Oryx/Renderer/Renderer.h"\n',
        "Oasis/src/Oasis/Core/OasisApp.h": '#include "Oasis/Game/G.h"\n',
        "Oasis/src/Oasis/Strategy/S.h": "\n",
    }.items():
        path = tmp_path / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
    messages = [v.message for v in OASIS.check(tmp_path)]
    assert messages == ["Game must not include Core (the app shell sits on top of the rules)", "Game must not include Oryx/Renderer (game rules and strategies stay headless)"]


def test_the_suite_names_both_trees():
    assert list(SUITE.specs) == ["oryx", "oasis"]


def test_gui_and_canvas_may_not_include_the_workbench_layer(tmp_path):
    src = _tree(tmp_path, {
        "Interface/GUI/Gui.h": '#include "Oryx/Interface/Workbench/Workbench.h"\n',
        "Interface/Canvas/Painter.h": '#include "Oryx/Interface/Workbench/Workbench.h"\n',
        "Interface/Workbench/Workbench.h": '#include "Oryx/Interface/GUI/Gui.h"\n',
    })
    assert len(check_includes(src)) == 2


def test_workbench_layer_knows_no_dashboard_devtools_or_assets(tmp_path):
    src = _tree(tmp_path, {
        "Interface/Workbench/A.h": '#include "Oryx/Dashboard/Dashboard.h"\n',
        "Interface/Workbench/B.h": '#include "Oryx/Devtools/ConsoleWorkbench.h"\n',
        "Interface/Workbench/C.h": '#include "Oryx/Assets/ImageAsset.h"\n',
        "Interface/Workbench/D.h": '#include "Oryx/Interface/Canvas/Painter.h"\n#include "Oryx/Core/Base.h"\n',
    })
    assert len(check_includes(src)) == 3


def test_devtools_build_on_the_workbench_layer_and_nothing_else_includes_them(tmp_path):
    src = _tree(tmp_path, {
        "Devtools/ConsoleWorkbench.h": '#include "Oryx/Interface/Workbench/Workbench.h"\n#include "Oryx/Debug/ConsoleCommands.h"\n',
        "Devtools/Bad.h": '#include "Oryx/Board/BoardLayer.h"\n',
        "Interface/GUI/Gui.h": '#include "Oryx/Devtools/ConsoleWorkbench.h"\n',
    })
    assert len(check_includes(src)) == 2
