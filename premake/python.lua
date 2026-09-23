-- With --no-python nothing here adds a file, include path, define or link; `uv run build` passes the --python-* paths.

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

-- build/generated/PythonConfig.h (written by `uv run build`) lets the embedded interpreter start with no environment setup.
function useOryxPythonEmbedding()
    if not pythonEnabled() then
        return
    end

    includedirs { "%{wks.location}/generated" }
end

-- Whole-archiving into the OryxPython shared library needs -fPIC on Linux.
function useOryxPythonPIC()
    if pythonEnabled() then
        filter "system:linux"
            pic "On"
        filter {}
    end
end

-- An extension must not link libpython: a second, uninitialised runtime copy crashes pybind11 (python-api.md, "Extension module").
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
