-- Shared per-project defaults. Included once from the root premake5.lua;
-- every project's premake5.lua then calls useOryxProjectDefaults() instead
-- of repeating language/cppdialect/staticruntime/targetdir/objdir lines.
function useOryxProjectDefaults()
    language "C++"
    cppdialect "C++20"
    staticruntime "off"

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")
end
