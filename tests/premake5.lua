-- Test executable
project "Tests"
    kind "ConsoleApp"
    useOryxProjectDefaults()

    pchheader "oxpch.h"
    pchsource "oxpch.cpp"

    files {
        "**.h",
        "**.hpp",
        "**.cpp",
    }

    -- vendor/doctest is the upstream doctest submodule: exclude its own
    -- tests/examples/scripts from our project, we only need its header.
    useVendorHeader("doctest", "doctest")

    -- Oasis's game/strategy sources are compiled in (not the Oasis executable's app/UI code) so their rules are unit-testable.
    files {
        "%{_MAIN_SCRIPT_DIR}/Oasis/src/Oasis/Game/TicTacToeGame.cpp",
        "%{_MAIN_SCRIPT_DIR}/Oasis/src/Oasis/Strategy/TicTacToeHeuristicStrategy.cpp",
    }

    includedirs {
        ".",
        "%{_MAIN_SCRIPT_DIR}/Oasis/src",
        "%{_MAIN_SCRIPT_DIR}/Oryx/src",
        "%{IncludeDir.spdlog}",
    }

    defines {
        "SPDLOG_COMPILED_LIB"
    }
    useOryxPython()

    linkOryxWholeArchive()
    links {
        "spdlog",
    }
    linkPython()
