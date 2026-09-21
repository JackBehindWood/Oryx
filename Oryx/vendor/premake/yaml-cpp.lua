-- Vendored static library: yaml-cpp
-- Source: Oryx/vendor/yaml-cpp/ (untouched upstream checkout — do not
-- add files there; edit this file instead).
project "yaml-cpp"
    kind "StaticLib"
    language "C++"
    staticruntime "off"
    warnings "Off"  -- third-party code; don't enforce our own warning level

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

    files {
        "../yaml-cpp/src/**.h",
        "../yaml-cpp/src/**.hpp",
        "../yaml-cpp/src/**.c",
        "../yaml-cpp/src/**.cpp",
    }

    includedirs {
        "../yaml-cpp/include"
    }
