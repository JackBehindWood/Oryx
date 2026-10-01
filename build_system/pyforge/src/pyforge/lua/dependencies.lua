-- forge's Premake side: forge.toml dependencies (config.json) turned into Premake projects.
forge = forge or {}

local config_cache

local function config()
    if config_cache == nil then
        local file = path.join(_MAIN_SCRIPT_DIR, "build", "forge", "config.json")
        local text = io.readfile(file)
        if not text then
            error("forge: " .. file .. " is missing; generate the build through `forge configure`")
        end
        config_cache = json.decode(text)
    end
    return config_cache
end

function forge.dependency(name)
    local dependency = (config().dependencies or {})[name]
    if not dependency then
        error("forge: dependency '" .. name .. "' is not in forge.toml [dependencies], or its `requires` are not met")
    end
    return dependency
end

function forge.include(name)
    return forge.dependency(name).include
end

-- Applies a dependency to the project being defined: include dirs/links for a built one, compiler flags for a system one.
function forge.use(name)
    local dependency = forge.dependency(name)
    if dependency.kind == "system" then
        buildoptions(dependency.cflags or {})
        linkoptions(dependency.libs or {})
        if #(dependency.defines or {}) > 0 then
            defines(dependency.defines)
        end
        return
    end
    includedirs { dependency.include }
    if dependency.kind == "static" then
        links { name }
    end
end

local dependency_callbacks = {}

-- fn(name, dependency) runs inside each generated dependency project, for project-specific settings.
function forge.on_dependency(fn)
    table.insert(dependency_callbacks, fn)
end

function forge.dependency_projects()
    local names = {}
    for name, dependency in pairs(config().dependencies or {}) do
        if dependency.kind == "static" then
            table.insert(names, name)
        end
    end
    table.sort(names)

    local output = outputdir or "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
    for _, name in ipairs(names) do
        local dependency = config().dependencies[name]
        local sources = dependency.sources or dependency.dir
        project(name)
            kind "StaticLib"
            language "C++"
            staticruntime "off"
            warnings "Off"

            targetdir ("%{wks.location}/bin/" .. output .. "/%{prj.name}")
            objdir ("%{wks.location}/bin-int/" .. output .. "/%{prj.name}")

            files {
                sources .. "/**.h",
                sources .. "/**.hpp",
                sources .. "/**.c",
                sources .. "/**.cpp",
            }

            includedirs { dependency.include }

            if #(dependency.defines or {}) > 0 then
                defines(dependency.defines)
            end

            for _, fn in ipairs(dependency_callbacks) do
                fn(name, dependency)
            end
    end
end
