-- The research host: `import oryx` from a standalone Python process; Oryx-only, so no Oasis games are compiled in.
project "OryxPython"
    kind "SharedLib"
    forge.project_defaults()

    targetname "oryx"
    targetprefix ""

    files {
        "src/**.h",
        "src/**.cpp",
    }

    includedirs {
        "%{_MAIN_SCRIPT_DIR}/Oryx/src",
        "%{_MAIN_SCRIPT_DIR}/Oryx/backends/Python",
        forge.include("spdlog"),
    }

    defines { "SPDLOG_COMPILED_LIB" }
    useOryxPython()
    useOryxPythonHeaders()

    -- Owns PyInit_oryx, init() and the atexit teardown; the bindings come from the whole libOryx.a archive.
    useOryxWholeArchive()
    links { "spdlog" }
    linkPythonExtension()

    filter "system:macosx or system:linux"
        targetextension ".so"
    filter {}
