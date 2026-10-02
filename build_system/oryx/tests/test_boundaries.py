import importlib.util
from pathlib import Path

_SPEC = importlib.util.spec_from_file_location("boundaries", Path(__file__).parents[1] / "boundaries.py")
boundaries = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(boundaries)

REPO_SRC = Path(__file__).resolve().parents[3] / "Oryx" / "src"


def _tree(tmp_path: Path, files: dict[str, str]) -> Path:
    for relative, content in files.items():
        path = tmp_path / "Oryx" / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
    return tmp_path


def test_real_tree_is_clean():
    assert boundaries.check_includes(REPO_SRC) == []


def test_headless_module_including_graphics_is_flagged(tmp_path):
    src = _tree(tmp_path, {"Game/G.h": '#include "Oryx/Renderer/Renderer.h"\n'})
    assert len(boundaries.check_includes(src)) == 1


def test_upward_layer_include_is_flagged(tmp_path):
    src = _tree(tmp_path, {"Graphics/RHI/A.h": '#include "Oryx/Shaders/Shaders.h"\n', "Renderer/R.h": '#include "Oryx/Shaders/Shaders.h"\n'})
    assert len(boundaries.check_includes(src)) == 1


def test_only_gpu_asset_cache_may_reach_graphics(tmp_path):
    src = _tree(tmp_path, {
        "Assets/GpuAssetCache.h": '#include "Oryx/Graphics/RHI/RHI.h"\n',
        "Assets/ImageAsset.h": '#include "Oryx/Graphics/RHI/RHI.h"\n',
    })
    assert len(boundaries.check_includes(src)) == 1


def test_public_header_including_backend_is_flagged(tmp_path):
    src = _tree(tmp_path, {"Core/X.h": '#include "backends/Null/NullRHI.h"\n'})
    assert len(boundaries.check_includes(src)) == 1


def test_cpu_assets_may_not_include_window_headers(tmp_path):
    src = _tree(tmp_path, {"Assets/ImageAsset.h": '#include "Oryx/Core/Window.h"\n', "Assets/GpuAssetCache.h": '#include "Oryx/Core/Window.h"\n'})
    assert len(boundaries.check_includes(src)) == 1


def test_core_window_and_input_headers_stay_graphics_free(tmp_path):
    src = _tree(tmp_path, {"Core/Window.h": '#include "Oryx/Renderer/GraphicsLayer.h"\n', "Core/Input.h": '#include "Oryx/Graphics/RHI/RHI.h"\n'})
    assert len(boundaries.check_includes(src)) == 2


def test_real_core_window_and_input_include_no_graphics_module():
    for name in ("Window.h", "Input.h", "PolledInput.h"):
        includes = boundaries._includes(REPO_SRC / "Oryx" / "Core" / name)
        assert not [i for i in includes if boundaries._module_of(i) in boundaries.GRAPHICS_MODULES]
