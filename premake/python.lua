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

newoption {
    trigger = "python-home",
    value = "path",
    description = "Prefix holding the interpreter's standard library, baked in as the embedded PYTHONHOME",
}

newoption {
    trigger = "python-package-dir",
    value = "path",
    description = "Directory holding the pure-Python oryx package, baked in as the embedded sys.path entry",
}

newoption {
    trigger = "python-site-packages",
    value = "paths",
    description = "Path-separator list of the venv's site-packages, baked in and appended to the embedded sys.path",
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

-- Baked into the backend so any binary linking Oryx can start the embedded interpreter with no env setup.
function useOryxPythonEmbedding()
    if not pythonEnabled() then
        return
    end

    defines {
        'OX_PYTHON_HOME="' .. requirePythonOption("python-home") .. '"',
        'OX_PYTHON_PACKAGE_DIR="' .. requirePythonOption("python-package-dir") .. '"',
        'OX_PYTHON_SITE_PACKAGES="' .. requirePythonOption("python-site-packages") .. '"',
    }
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
