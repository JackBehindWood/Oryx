-- Test executable
project "Tests"
    kind "ConsoleApp"
    useOryxProjectDefaults()

    files {
        "**.h",
        "**.hpp",
        "**.cpp",
    }

    -- vendor/doctest is the upstream doctest submodule: exclude its own
    -- tests/examples/scripts from our project, we only need its header.
    useVendorHeader("doctest", "doctest")

    includedirs {
        ".",
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
