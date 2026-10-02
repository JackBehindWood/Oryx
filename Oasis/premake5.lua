-- Oasis: sandbox executable for games/experiments built on top of Oryx
project "Oasis"
    kind "ConsoleApp"
    forge.project_defaults()

    pchheader "ospch.h"
    pchsource "src/ospch.cpp"

    -- Run with the repo root as cwd (matches .vscode/launch.json and the
    -- `uv run forge` CLI) so relative paths behave the same everywhere.
    debugdir "%{wks.location}/.."

    files {
        "src/**.h",
        "src/**.hpp",
        "src/**.cpp"
    }

    if not graphicsEnabled() then
        removefiles { "src/Oasis/Graphics/**" }
    end

    includedirs {
        "src",
        "%{_MAIN_SCRIPT_DIR}/Oryx/src",
        forge.include("spdlog"),
    }

    defines {
        "SPDLOG_COMPILED_LIB"
    }

    useOryxGraphics()

    useOryxWholeArchive()
    useOryxAllocationCensus()
    links {
        "spdlog",
    }
    linkPython()
