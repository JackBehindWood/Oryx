-- Oryx: An open-source engine for games, strategies, simulation, and decision-making

workspace "oryx"
	startproject "Oryx"

    configurations { "Debug", "Release", "Dist" }
    location "."

    if os.host() == "macosx" then
        platforms { "ARM64", "x64" }  -- ARM64 first (preferred on Apple Silicon)
    else
        platforms { "x64" }
    end

    filter "configurations:Debug"
        defines { "ORYX_DEBUG" }
        symbols "On"
        optimize "Off"

    filter "configurations:Release"
        defines { "ORYX_RELEASE" }
        optimize "On"

    filter "configurations:Dist"
        defines { "ORYX_DIST" }
        optimize "On"

    filter {}


outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

group "Core"
	include "Oryx"
group ""

group "Tests"
    include "tests"
group ""
