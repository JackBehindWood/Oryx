-- Shared per-project defaults. Included once from the root premake5.lua;
-- every project's premake5.lua then calls useOryxProjectDefaults() instead
-- of repeating language/cppdialect/staticruntime/targetdir/objdir lines.
function useOryxProjectDefaults()
    language "C++"
    cppdialect "C++20"
    staticruntime "off"

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")
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
