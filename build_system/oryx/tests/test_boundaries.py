import tomllib
from pathlib import Path

from pyforge.boundaries import Boundaries
from pyforge.config import parse_config

REPO = Path(__file__).resolve().parents[3]
CONFIG = parse_config(tomllib.loads((REPO / "forge.toml").read_text()))
BOUNDARIES = Boundaries(CONFIG.boundaries)


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
        "Assets/GpuShaderStore.h": '#include "Oryx/Shaders/ShaderBinaryStore.h"\n',
        "Assets/ShaderAsset.h": '#include "Oryx/Shaders/ShaderBinaryStore.h"\n',
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
