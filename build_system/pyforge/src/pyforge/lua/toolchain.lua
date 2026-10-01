-- forge's Premake side: shared project defaults, sanitizers, vendoring and linking helpers.
forge = forge or {}

newoption {
    trigger = "sanitize",
    description = "Build with AddressSanitizer + UndefinedBehaviorSanitizer (opt-in dev/CI tool; keeps the profile's optimize level so it still exercises Release/Dist codegen, forces debug symbols on for readable reports; the allocation census stays linked, so ASan's new/delete-mismatch check is inactive)",
}

-- Keeps the profile's optimize level on purpose: some bugs only reproduce with the optimizer (decision log, "Sanitizer builds").
function forge.sanitizers()
    if _OPTIONS["sanitize"] == nil then
        return
    end

    buildoptions { "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-fno-sanitize-recover=undefined" }
    linkoptions { "-fsanitize=address,undefined" }
    symbols "On"
end

-- Shared per-project defaults, so each project doesn't repeat language/dialect/runtime/output-dir lines.
function forge.project_defaults()
    language "C++"
    cppdialect "C++20"
    staticruntime "off"

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

    forge.sanitizers()
end

-- Generalizes the vendoring pattern (a header-only lib checked out as a git
-- submodule under <project>/vendor/<lib>/) so any project can vendor one
-- with a single line instead of hand-written includedirs/removefiles. By
-- default the header is assumed to sit directly under vendor/<lib>/ (the
-- common case for single-header libs). Pass `headerSubdir` for a submodule
-- whose own repo layout nests the real header one level down (e.g.
-- doctest: vendor/doctest/doctest/doctest.h -> forge.header_dependency("doctest", "doctest")).
function forge.header_dependency(libName, headerSubdir)
    local dir = "vendor/" .. libName
    if headerSubdir and headerSubdir ~= "" then
        dir = dir .. "/" .. headerSubdir
    end
    includedirs { dir }
    removefiles { "vendor/" .. libName .. "/**" }
end

-- Whole-archive linking, because self-registering objects have no other referenced symbol and a
-- plain `links {lib}` would drop them. `extra` are additional plain links (e.g. a library `lib`
-- pulls in transitively) that don't themselves need whole-archiving.
function forge.whole_archive(lib, extra)
    -- A project link (not just the linkoptions below) makes the executable relink whenever the library changes.
    links { lib }
    links(extra or {})

    filter "system:windows"
        linkoptions { "/WHOLEARCHIVE:" .. lib .. ".lib" }

    filter "system:macosx"
        linkoptions { "-force_load \"%{wks.location}/bin/" .. outputdir .. "/" .. lib .. "/lib" .. lib .. ".a\"" }

    filter "system:linux"
        links { "pthread" }
        linkoptions { "-Wl,--whole-archive", "%{wks.location}/bin/" .. outputdir .. "/" .. lib .. "/lib" .. lib .. ".a", "-Wl,--no-whole-archive" }

    filter {}
end
