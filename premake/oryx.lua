-- Oryx-specific Premake helpers (project-owned, not part of the generic forge.lua).

-- Executables only: a library (libOryx.a, the oryx extension) must not replace operator new inside its host process.
function useOryxAllocationCensus()
    files { "%{_MAIN_SCRIPT_DIR}/Oryx/backends/Program/AllocationCensus.cpp" }
    includedirs { "%{_MAIN_SCRIPT_DIR}/Oryx/src" }

    filter "files:**/AllocationCensus.cpp"
        flags { "NoPCH" }
    filter {}
end

function useOryxWholeArchive()
    forge.whole_archive("Oryx", { "yaml-cpp" })
end
