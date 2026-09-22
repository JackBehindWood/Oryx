-- Python is on by default and opt-out (`--no-python`). Only Oryx/backends/Python
-- needs pybind11 and <Python.h>; with it off nothing here adds a Python file,
-- include path, define or link. `uv run build` passes the interpreter's paths
-- (read from sysconfig) as the --python-* options.

newoption {
    trigger = "no-python",
    description = "Build without the Python scripting backend",
}

newoption {
    trigger = "python-include",
    value = "path",
    description = "Directory containing Python.h",
}

newoption {
    trigger = "python-libdir",
    value = "path",
    description = "Directory containing the Python shared library",
}

newoption {
    trigger = "python-lib",
    value = "name",
    description = "Python library to link, without the lib prefix or extension (e.g. python3.11)",
}

function pythonEnabled()
    return _OPTIONS["no-python"] == nil
end

local function requirePythonOption(name)
    local value = _OPTIONS[name]
    if not value then
        error("Python is enabled but --" .. name .. " was not given; run through `uv run build` or pass --no-python")
    end
    return value
end

-- For code that only needs to know Python is built in (e.g. tests gated on OX_ENABLE_PYTHON).
function useOryxPython()
    if pythonEnabled() then
        defines { "OX_ENABLE_PYTHON" }
    end
end

-- For code that includes Python.h or pybind11.
function useOryxPythonHeaders()
    if not pythonEnabled() then
        return
    end

    includedirs {
        "%{IncludeDir.pybind11}",
        requirePythonOption("python-include"),
    }
end

-- The embedded interpreter's home and site-packages live in build/generated/PythonConfig.h (written by
-- `uv run build`), so any binary linking Oryx can start it with no env setup; only PythonRuntime.cpp includes it.
function useOryxPythonEmbedding()
    if not pythonEnabled() then
        return
    end

    includedirs { "%{wks.location}/generated" }
end

-- Whole-archiving Oryx/yaml-cpp/spdlog into OryxPython needs -fPIC on Linux. Gated on Python
-- being enabled so a Python-off Linux build is untouched.
function useOryxPythonPIC()
    if pythonEnabled() then
        filter "system:linux"
            pic "On"
        filter {}
    end
end

-- OryxPython is a plain Python extension module, not an embedding host like Oasis/Tests: it must
-- NOT link libpython directly. Doing so gives it its own separate, never-initialised copy of the
-- interpreter's runtime state (distinct from the one the importing process already set up), and
-- pybind11's PyGILState_Ensure() segfaults dereferencing that copy's uninitialised _PyRuntime.
-- Its Python C-API symbol references stay unresolved at link time and are satisfied by the host
-- process at import (dlopen) instead - the same as every other compiled Python extension.
function linkPythonExtension()
    if not pythonEnabled() then
        return
    end

    filter "system:macosx"
        linkoptions { "-undefined dynamic_lookup" }
    filter {}
end

-- Whole-archive linking pulls the backend's libpython references into every consumer of Oryx.
function linkPython()
    if not pythonEnabled() then
        return
    end

    libdirs { requirePythonOption("python-libdir") }
    links { requirePythonOption("python-lib") }

    filter "system:not windows"
        linkoptions { "-Wl,-rpath," .. requirePythonOption("python-libdir") }

    filter {}
end
