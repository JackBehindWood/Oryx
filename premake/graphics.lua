-- With --no-graphics nothing here adds a file, include path, define or link.

newoption {
    trigger = "no-graphics",
    description = "Build without window backends, the RHI and the 2D renderer",
}

function graphicsEnabled()
    return _OPTIONS["no-graphics"] == nil
end

function useOryxGraphics()
    if graphicsEnabled() then
        defines { "OX_ENABLE_GRAPHICS" }
    end
end

-- forge's generic static-dependency project only globs .c/.cpp; GLFW needs its Cocoa .m files and a platform define, and only macOS builds it so far.
function useOryxGlfw(name, dependency)
    if name ~= "glfw" then
        return
    end

    filter "system:not macosx"
        kind "None"

    filter "system:macosx"
        defines { "_GLFW_COCOA" }
        files { dependency.sources .. "/**.m" }
        removefiles {
            dependency.sources .. "/win32_*",
            dependency.sources .. "/wgl_*",
            dependency.sources .. "/x11_*",
            dependency.sources .. "/xkb_*",
            dependency.sources .. "/glx_*",
            dependency.sources .. "/wl_*",
            dependency.sources .. "/linux_*",
            dependency.sources .. "/posix_poll.*",
        }
        buildoptions { "-fno-objc-arc" }
    filter {}
end

-- libOryx is static, so each final binary links GLFW and the system frameworks itself.
function linkOryxGraphics()
    if graphicsEnabled() then
        filter "system:macosx"
            links { "glfw", "Metal.framework", "QuartzCore.framework", "Foundation.framework", "AppKit.framework", "IOKit.framework", "Cocoa.framework", "CoreFoundation.framework" }
        filter {}
    end
end
