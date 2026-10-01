project "Oryx"
    kind "StaticLib"
    forge.project_defaults()

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

    -- File-scoped so a new commit recompiles one file, not the library.
    filter "files:src/Oryx/Simulation/BuildInfo.cpp"
        local hash = os.outputof("git rev-parse --short=12 HEAD")
        if hash and hash:match("^%x+$") then
            defines { "OX_GIT_HASH=" .. hash }
        end
    filter {}

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
