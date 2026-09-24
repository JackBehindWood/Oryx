import pytest

from build_system.setup import vendor_scaffold

ROOT_PREMAKE = 'workspace "Oryx"\n\ngroup "Core"\n    include "Oryx"\ngroup ""\n'
DEPENDENCIES_LUA = 'IncludeDir = {}\nIncludeDir["spdlog"] = "%{_MAIN_SCRIPT_DIR}/Oryx/vendor/spdlog/include"\n'
CONSUMER_PREMAKE = 'project "Oasis"\n    kind "ConsoleApp"\n    useOryxProjectDefaults()\n\n    includedirs {\n        "src",\n    }\n'


@pytest.fixture
def lua(tmp_project):
    files = {
        "premake5.lua": ROOT_PREMAKE,
        "premake/dependencies.lua": DEPENDENCIES_LUA,
        "Oasis/premake5.lua": CONSUMER_PREMAKE,
    }
    for name, text in files.items():
        (tmp_project / name).parent.mkdir(parents=True, exist_ok=True)
        (tmp_project / name).write_text(text, encoding="utf-8")
    return lambda name: (tmp_project / name).read_text(encoding="utf-8")


def test_paths(tmp_project):
    assert vendor_scaffold.vendor_path("Oryx", "glfw") == tmp_project / "Oryx" / "vendor" / "glfw"
    assert vendor_scaffold.premake_script_path("Oryx", "glfw") == tmp_project / "Oryx" / "vendor" / "premake" / "glfw.lua"


def test_static_lib_script(tmp_project):
    path = vendor_scaffold.write_static_lib_script("Oryx", "glfw", include_subdir="include", source_subdir="src", defines=["_GLFW_COCOA", "GLFW_X"])
    assert path == tmp_project / "Oryx" / "vendor" / "premake" / "glfw.lua"
    assert path.read_text(encoding="utf-8") == """\
-- Vendored static library: glfw
-- Source: Oryx/vendor/glfw/ (untouched upstream checkout — do not
-- add files there; edit this file instead).
project "glfw"
    kind "StaticLib"
    language "C++"
    staticruntime "off"
    warnings "Off"  -- third-party code; don't enforce our own warning level

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

    files {
        "../glfw/src/**.h",
        "../glfw/src/**.hpp",
        "../glfw/src/**.c",
        "../glfw/src/**.cpp",
    }

    includedirs {
        "../glfw/include"
    }

    defines {
        "_GLFW_COCOA",
        "GLFW_X",
    }
"""


def test_static_lib_script_without_subdirs_or_defines(tmp_project):
    text = vendor_scaffold.write_static_lib_script("Oryx", "stb").read_text(encoding="utf-8")
    assert '"../stb/**.cpp",' in text
    assert text.endswith('    includedirs {\n        "../stb"\n    }\n')


@pytest.mark.parametrize(("subdir", "call"), [(None, 'useVendorHeader("stb")'), ("doctest", 'useVendorHeader("doctest", "doctest")')])
def test_header_only_usage_is_idempotent(lua, subdir, call):
    name = "doctest" if subdir else "stb"
    assert vendor_scaffold.insert_header_only_usage("Oasis", name, subdir) == (True, call)
    once = lua("Oasis/premake5.lua")
    assert once == CONSUMER_PREMAKE.replace("useOryxProjectDefaults()", f"useOryxProjectDefaults()\n\n    {call}")
    assert vendor_scaffold.insert_header_only_usage("Oasis", name, subdir) == (True, call)
    assert lua("Oasis/premake5.lua") == once


def test_header_only_usage_without_anchor_leaves_file(lua, tmp_project):
    (tmp_project / "Oasis/premake5.lua").write_text('project "Oasis"\n', encoding="utf-8")
    assert vendor_scaffold.insert_header_only_usage("Oasis", "stb", None) == (False, 'useVendorHeader("stb")')
    assert lua("Oasis/premake5.lua") == 'project "Oasis"\n'


def test_insert_into_brace_list():
    assert vendor_scaffold._insert_into_brace_list('links {\n    "a",\n}\n', "links", "b") == 'links {\n        "b",\n    "a",\n}\n'
    assert vendor_scaffold._insert_into_brace_list("kind 'x'\n", "links", "b") is None


def test_insert_or_append_block():
    assert vendor_scaffold._insert_or_append_block('project "P"\n', "links", ["a", "b"]) == 'project "P"\n\n    links {\n        "a",\n        "b",\n    }\n'
    assert vendor_scaffold._insert_or_append_block('links {\n}\n', "links", ["a", "b"]) == 'links {\n        "b",\n        "a",\n}\n'


ROOT_WIRED = 'workspace "Oryx"\n\ngroup "Dependencies"\n    include "Oasis/vendor/premake/glfw.lua"\ngroup ""\n\ngroup "Core"\n    include "Oryx"\ngroup ""\n'
DEPENDENCIES_WIRED = DEPENDENCIES_LUA.replace("IncludeDir = {}\n", 'IncludeDir = {}\nIncludeDir["glfw"] = "%{_MAIN_SCRIPT_DIR}/Oasis/vendor/glfw/include"\n')


def test_static_lib_wiring(lua):
    edited, snippets = vendor_scaffold.insert_static_lib_wiring("Oasis", "glfw", include_subdir="include", defines=["GLFW_X"])
    assert edited is True
    assert snippets == [
        'include "Oasis/vendor/premake/glfw.lua"  # in the root premake5.lua\'s group "Dependencies"',
        'IncludeDir["glfw"] = "%{_MAIN_SCRIPT_DIR}/Oasis/vendor/glfw/include"  # in premake/dependencies.lua',
        '"%{IncludeDir["glfw"]}"  # add to Oasis/premake5.lua\'s includedirs { ... } block',
        '"glfw"  # add to Oasis/premake5.lua\'s links { ... } block (create one if none exists)',
        '"GLFW_X"  # add to Oasis/premake5.lua\'s defines { ... } block (create one if none exists)',
    ]
    assert lua("premake5.lua") == ROOT_WIRED
    assert lua("premake/dependencies.lua") == DEPENDENCIES_WIRED
    assert lua("Oasis/premake5.lua") == (
        'project "Oasis"\n    kind "ConsoleApp"\n    useOryxProjectDefaults()\n\n'
        "    includedirs {\n        \"%{IncludeDir['glfw']}\",\n        \"src\",\n    }\n\n"
        '    links {\n        "glfw",\n    }\n\n'
        '    defines {\n        "GLFW_X",\n    }\n'
    )


def test_static_lib_wiring_repeats_consumer_entries_on_rerun(lua):
    vendor_scaffold.insert_static_lib_wiring("Oasis", "glfw", include_subdir="include")
    vendor_scaffold.insert_static_lib_wiring("Oasis", "glfw", include_subdir="include")
    assert lua("premake5.lua") == ROOT_WIRED
    assert lua("premake/dependencies.lua") == DEPENDENCIES_WIRED
    consumer = lua("Oasis/premake5.lua")
    assert consumer.count("%{IncludeDir['glfw']}") == 2
    assert consumer.count('"glfw",') == 2


def test_static_lib_wiring_reuses_existing_dependencies_group(lua, tmp_project):
    (tmp_project / "premake5.lua").write_text('group "Dependencies"\n    include "x.lua"\ngroup ""\n', encoding="utf-8")
    vendor_scaffold.insert_static_lib_wiring("Oasis", "glfw")
    assert lua("premake5.lua") == 'group "Dependencies"\n    include "Oasis/vendor/premake/glfw.lua"\n    include "x.lua"\ngroup ""\n'


@pytest.mark.parametrize(
    ("name", "text"),
    [("premake5.lua", 'workspace "Oryx"\n'), ("premake/dependencies.lua", "-- no table\n"), ("Oasis/premake5.lua", 'project "Oasis"\n')],
)
def test_static_lib_wiring_without_anchor_is_not_edited(lua, tmp_project, name, text):
    (tmp_project / name).write_text(text, encoding="utf-8")
    edited, snippets = vendor_scaffold.insert_static_lib_wiring("Oasis", "glfw")
    assert edited is False
    assert len(snippets) == 4
    assert lua("Oasis/premake5.lua") in (CONSUMER_PREMAKE, 'project "Oasis"\n')


def test_add_git_submodule(tmp_project, monkeypatch):
    calls = []
    monkeypatch.setattr(vendor_scaffold, "run_command", lambda command, cwd=None: calls.append((command, cwd)))
    vendor_scaffold.add_git_submodule("https://github.com/glfw/glfw.git", "Oryx", "glfw")
    assert calls == [(["git", "submodule", "add", "https://github.com/glfw/glfw.git", str(vendor_scaffold.vendor_path("Oryx", "glfw").relative_to(tmp_project))], tmp_project)]


def test_cli_rejects_unknown_project(forge):
    result = forge("vendor", "add", "Nowhere", "glfw")
    assert result.exit_code == 1
    assert "Unknown project 'Nowhere'. Must be one of: Oryx, Oasis, tests" in result.output


def test_cli_requires_url_for_missing_checkout(forge):
    result = forge("vendor", "add", "Oasis", "glfw")
    assert result.exit_code == 1
    assert "Oasis/vendor/glfw doesn't exist yet. Pass --url to add it as a submodule." in result.output


def test_cli_header_only_on_existing_checkout(forge, lua, tmp_project):
    (tmp_project / "Oasis/vendor/stb").mkdir(parents=True)
    result = forge("vendor", "add", "Oasis", "stb")
    assert result.exit_code == 0, result.output
    assert 'useVendorHeader("stb")' in lua("Oasis/premake5.lua")
    assert "5 vendored libraries tracked." in result.output
