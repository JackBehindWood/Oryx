-- Oryx Dependencies
--
-- Centralizes vendored library include paths referenced by multiple
-- projects, instead of hardcoding each one per-project (mirrors Hazel's
-- Dependencies.lua). Compiled vendor libs additionally get their own
-- project under the root workspace's "Dependencies" group — see
-- <project>/vendor/premake/<lib>.lua — wired up alongside an entry here by
-- `uv run forge vendor add ... --kind static-lib`.

IncludeDir = {}
IncludeDir["yaml-cpp"] = "%{_MAIN_SCRIPT_DIR}/Oryx/vendor/yaml-cpp/include"
IncludeDir["spdlog"] = "%{_MAIN_SCRIPT_DIR}/Oryx/vendor/spdlog/include"
IncludeDir["pybind11"] = "%{_MAIN_SCRIPT_DIR}/Oryx/vendor/pybind11/include"
