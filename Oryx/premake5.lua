project "Oryx"
    kind "StaticLib"
    useOryxProjectDefaults()

    pchheader "oxpch.h"
	pchsource "src/oxpch.cpp"

    files {
        "src/**.h",
        "src/**.hpp",
        "src/**.cpp"
    }

    includedirs {
        forge.include("yaml-cpp"),
        forge.include("spdlog"),
        "src"
    }

    links {
        "yaml-cpp",
        "spdlog",
    }

    defines {
        "SPDLOG_COMPILED_LIB",
    }

    useOryxPythonPIC()

    if pythonEnabled() then
        files {
            "backends/Python/**.h",
            "backends/Python/**.hpp",
            "backends/Python/**.cpp"
        }
        includedirs { "backends/Python" }
        useOryxPython()
        useOryxPythonHeaders()
        useOryxPythonEmbedding()
    end
