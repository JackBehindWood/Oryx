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
        "%{IncludeDir.spdlog}",
        "src"
    }

    links {
        "spdlog",
    }

    defines {
        "SPDLOG_COMPILED_LIB",
    }
