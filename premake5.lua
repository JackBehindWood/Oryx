-- Oryx: An open-source engine for games, strategies, simulation, and decision-making

include "premake/common.lua"
include "premake/vendor.lua"
include "premake/dependencies.lua"
include "premake/python.lua"

workspace "oryx"
	startproject "Oryx"

    configurations { "Debug", "Release", "Dist" }
    location "build"
    warnings "Extra"

    if os.host() == "macosx" then
        platforms { "ARM64", "x64" }  -- ARM64 first (preferred on Apple Silicon)
    else
        platforms { "x64" }
    end

    filter "configurations:Debug"
        defines { "OX_DEBUG", "OX_ENABLE_PROFILING", "OX_ENABLE_MEMORY_TRACKING" }
        symbols "On"
        optimize "Off"

    filter "configurations:Release"
        defines { "OX_RELEASE", "OX_ENABLE_PROFILING", "OX_ENABLE_MEMORY_TRACKING" }
        optimize "On"

    filter "configurations:Dist"
        defines { "OX_DIST" }
        optimize "On"

    -- Pin the Windows SDK to whatever's newest on the machine, rather than
    -- letting Premake/VS silently pick an arbitrary installed version.
    filter "system:windows"
        systemversion "latest"

    filter {}


outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

group "Dependencies"
    include "Oryx/vendor/premake/yaml-cpp.lua"
    include "Oryx/vendor/premake/spdlog.lua"
group ""

group "Core"
	include "Oryx"
group ""

group "Apps"
    include "Oasis"
group ""

group "Bindings"
    if pythonEnabled() then
        include "OryxPython"
    end
group ""

group "Tests"
    include "tests"
group ""
