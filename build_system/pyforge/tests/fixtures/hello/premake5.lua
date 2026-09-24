-- A tiny, non-Oryx project: proves pyforge drives an arbitrary Premake project, not just Oryx's.
require "forge"

workspace "hello"
    configurations { "Debug", "Release", "Dist" }
    location "build"

    filter "configurations:Debug"
        symbols "On"
        optimize "Off"
    filter "configurations:Release"
        optimize "On"
    filter "configurations:Dist"
        optimize "On"
    filter {}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

project "hello"
    kind "ConsoleApp"
    forge.project_defaults()
    files { "src/**.cpp", "src/**.h" }
