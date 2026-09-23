-- Shared per-project defaults, so each project doesn't repeat language/dialect/runtime/output-dir lines.
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
    description = "Build with AddressSanitizer + UndefinedBehaviorSanitizer (opt-in dev/CI tool; keeps the profile's optimize level so it still exercises Release/Dist codegen, forces debug symbols on for readable reports; the allocation census stays linked, so ASan's new/delete-mismatch check is inactive)",
}

-- Keeps the profile's optimize level on purpose: some bugs only reproduce with the optimizer (decision log, "Sanitizer builds").
function useOryxSanitizers()
    if _OPTIONS["sanitize"] == nil then
        return
    end

    buildoptions { "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-fno-sanitize-recover=undefined" }
    linkoptions { "-fsanitize=address,undefined" }
    symbols "On"
end

-- Executables only: a library (libOryx.a, the oryx extension) must not replace operator new inside its host process.
function useOryxAllocationCensus()
    files { "%{_MAIN_SCRIPT_DIR}/Oryx/backends/Program/AllocationCensus.cpp" }
    includedirs { "%{_MAIN_SCRIPT_DIR}/Oryx/src" }

    filter "files:**/AllocationCensus.cpp"
        flags { "NoPCH" }
    filter {}
end

-- Whole archive, because self-registering objects have no other referenced symbol and a plain `links "Oryx"` would drop them.
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
