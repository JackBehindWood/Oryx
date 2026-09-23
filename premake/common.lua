-- Shared per-project defaults. Included once from the root premake5.lua;
-- every project's premake5.lua then calls useOryxProjectDefaults() instead
-- of repeating language/cppdialect/staticruntime/targetdir/objdir lines.
function useOryxProjectDefaults()
    language "C++"
    cppdialect "C++20"
    staticruntime "off"

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

    useOryxSanitizers()
end

newoption {
    trigger = "sanitize",
    description = "Build with AddressSanitizer + UndefinedBehaviorSanitizer (opt-in dev/CI tool; keeps the profile's own optimize/symbols settings, so it still exercises Release/Dist codegen when built with --profile release|dist)",
}

-- Opt-in, keeps whatever optimize/symbols the active configuration already has - a Release/Dist
-- build under --sanitize still runs through the optimizer, which is the point: some bugs only
-- reproduce once the optimizer is on, and a sanitizer build is how you catch them at the actual
-- faulting read/write instead of wherever the corruption happens to be noticed downstream.
function useOryxSanitizers()
    if _OPTIONS["sanitize"] == nil then
        return
    end

    buildoptions { "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-fno-sanitize-recover=undefined" }
    linkoptions { "-fsanitize=address,undefined" }
    symbols "On"
end

-- Games/strategies self-register via a static object with no other
-- externally-visible symbol (docs/architecture.md §10). A plain `links "Oryx"`
-- lets the linker silently drop object files from the libOryx.a archive that
-- nothing else references, so those registrations never run - use this
-- instead of `links "Oryx"` in any project that consumes Oryx's registry
-- without directly naming every concrete type (Oasis, Tests).
function linkOryxWholeArchive()
    -- A project link (not just the linkoptions below) makes the executable relink whenever libOryx changes.
    links { "Oryx", "yaml-cpp" }

    filter "system:windows"
        linkoptions { "/WHOLEARCHIVE:Oryx.lib" }

    filter "system:macosx"
        linkoptions { "-force_load \"%{wks.location}/bin/" .. outputdir .. "/Oryx/libOryx.a\"" }

    filter "system:linux"
        links { "pthread" }
        linkoptions { "-Wl,--whole-archive", "%{wks.location}/bin/" .. outputdir .. "/Oryx/libOryx.a", "-Wl,--no-whole-archive" }

    filter {}
end
