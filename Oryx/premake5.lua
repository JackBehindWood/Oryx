project "Oryx"
    kind "StaticLib"
    useOryxProjectDefaults()

    files {
        "src/**.h",
        "src/**.hpp",
        "src/**.cpp"
    }

    includedirs {
        "%{IncludeDir.spdlog}",
        "src"
    }

    links {
        "spdlog",
    }

    defines {
        "SPDLOG_COMPILED_LIB",
    }
