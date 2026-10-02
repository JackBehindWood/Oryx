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
        forge.include("stb"),
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

    useOryxGraphics()

    files {
        "backends/Null/NullWindow.h",
        "backends/Null/NullWindow.cpp"
    }
    includedirs { "backends/Null" }

    if graphicsEnabled() then
        files {
            "backends/Null/NullRHI.h",
            "backends/Null/NullRHI.cpp"
        }

        filter "system:macosx"
            files {
                "backends/Metal/**.h",
                "backends/Metal/**.cpp",
                "backends/Metal/**.mm",
                "backends/MacOS/**.h",
                "backends/MacOS/**.mm"
            }
            includedirs {
                "backends/Metal",
                "backends/MacOS",
                forge.include("glfw"),
                forge.include("metal-cpp")
            }
            links { "Metal.framework", "QuartzCore.framework", "Foundation.framework", "AppKit.framework", "IOKit.framework", "Cocoa.framework", "CoreFoundation.framework", "CoreGraphics.framework" }

        filter "files:backends/Metal/**.cpp"
            flags { "NoPCH" }

        -- metal-cpp's NS::SharedPtr deliberately messages nil (a no-op) on null objects, which UBSan's null check flags; RHI.cpp instantiates it through MetalRHI.h.
        filter { "files:backends/Metal/**.cpp or src/Oryx/Graphics/RHI/RHI.cpp" }
            buildoptions { "-fno-sanitize=null" }

        filter "files:backends/Metal/**.mm"
            flags { "NoPCH" }
            buildoptions { "-fobjc-arc" }

        filter "files:backends/MacOS/**.mm"
            flags { "NoPCH" }
            buildoptions { "-fobjc-arc" }
        filter {}
    else
        removefiles {
            "src/Oryx/Graphics/**",
            "src/Oryx/Shaders/**",
            "src/Oryx/Renderer/**",
            "src/Oryx/Assets/GpuAssetCache*"
        }
    end
