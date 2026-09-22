-- Vendored static library: spdlog
-- Source: Oryx/vendor/spdlog/ (untouched upstream checkout — do not
-- add files there; edit this file instead).
project "spdlog"
    kind "StaticLib"
    language "C++"
    staticruntime "off"
    warnings "Off"  -- third-party code; don't enforce our own warning level

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

    files {
        "../spdlog/src/**.h",
        "../spdlog/src/**.hpp",
        "../spdlog/src/**.c",
        "../spdlog/src/**.cpp",
    }

    includedirs {
        "../spdlog/include"
    }

    defines {
        "SPDLOG_COMPILED_LIB",
    }

    useOryxPythonPIC()
