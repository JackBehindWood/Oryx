-- Oasis: sandbox executable for games/experiments built on top of Oryx
project "Oasis"
    kind "ConsoleApp"
    useOryxProjectDefaults()

    pchheader "ospch.h"
    pchsource "src/ospch.cpp"

    -- Run with the repo root as cwd (matches .vscode/launch.json and the
    -- `uv run build` CLI) so relative paths behave the same everywhere.
    debugdir "%{wks.location}/.."

    files {
        "src/**.h",
        "src/**.hpp",
        "src/**.cpp"
    }

    includedirs {
        "src",
        "%{_MAIN_SCRIPT_DIR}/Oryx/src",
        "%{IncludeDir.spdlog}",
    }

    defines {
        "SPDLOG_COMPILED_LIB"
    }

    linkOryxWholeArchive()
    links {
        "spdlog",
    }
