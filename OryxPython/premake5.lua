-- The research-host extension: `import oryx` from a standalone Python process (REPL, `uv run
-- python`, Jupyter). Oryx-only: no Oasis coupling, so TicTacToe (compiled only in Oasis) isn't
-- reachable from a bare `import oryx`; Nim and the Monte Carlo strategy still are, since those
-- are plain Python scripts, unaffected by this build target.
project "OryxPython"
    kind "SharedLib"
    useOryxProjectDefaults()

    targetname "oryx"
    targetprefix ""

    files {
        "src/**.h",
        "src/**.cpp",
    }

    includedirs {
        "%{_MAIN_SCRIPT_DIR}/Oryx/src",
        "%{IncludeDir.spdlog}",
    }

    defines { "SPDLOG_COMPILED_LIB" }
    useOryxPython()

    -- Everything real (bindings, PyInit_oryx) already lives in libOryx.a; this target just
    -- re-links that whole archive as a shared object Python's import machinery can dlopen -
    -- Oryx is a static library and Python can only import shared libraries.
    linkOryxWholeArchive()
    links { "spdlog" }
    linkPythonExtension()

    filter "system:macosx or system:linux"
        targetextension ".so"
    filter {}
