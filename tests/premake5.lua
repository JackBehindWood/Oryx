-- Test executable
project "TESTS"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    
    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

    files {
        "**.h",
        "**.hpp",
        "**.cpp",
    }

    includedirs {
        ".",
    }

    links {
        "ORYX"
    }