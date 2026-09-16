-- Oasis: sandbox executable for games/experiments built on top of Oryx
project "Oasis"
    kind "ConsoleApp"
    useOryxProjectDefaults()

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

    links {
        "Oryx",
        "spdlog",
    }
