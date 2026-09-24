-- forge's Premake side: exports the resolved workspace to build/forge/workspace.json for the Python tooling.
forge = forge or {}

local p = premake

newoption {
    trigger = "forge-export",
    description = "Also write build/forge/workspace.json after the action runs (passed by forge configure)",
}

local function list(values)
    local result = {}
    for _, value in ipairs(values or {}) do
        table.insert(result, value)
    end
    return result
end

local function config_entry(cfg)
    return {
        buildcfg = cfg.buildcfg,
        platform = cfg.platform or "",
        architecture = cfg.architecture or "",
        kind = cfg.kind,
        token = cfg.shortname,
        target = cfg.buildtarget.abspath,
        targetdir = cfg.buildtarget.directory,
        objdir = cfg.objdir,
        defines = list(cfg.defines),
        includedirs = list(cfg.includedirs),
    }
end

local function project_entry(prj)
    local configs, files, seen = {}, {}, {}
    for cfg in p.project.eachconfig(prj) do
        table.insert(configs, config_entry(cfg))
        for _, file in ipairs(cfg.files) do
            if not seen[file] then
                seen[file] = true
                table.insert(files, file)
            end
        end
    end
    table.sort(files)
    return {
        basedir = prj.basedir,
        script = prj.script,
        group = prj.group or "",
        configs = configs,
        files = files,
    }
end

local function loaded_scripts()
    local scripts = { _MAIN_SCRIPT }
    for script in pairs(io._includedFiles or {}) do
        if not script:startswith("$/") then
            table.insert(scripts, script)
        end
    end
    table.sort(scripts)
    return scripts
end

function forge.export(wks)
    local projects = {}
    for prj in p.workspace.eachproject(wks) do
        projects[prj.name] = project_entry(prj)
    end
    local document = {
        format = 1,
        workspace = wks.name,
        location = wks.location,
        action = _ACTION,
        scripts = loaded_scripts(),
        projects = projects,
    }
    local file = path.join(wks.location, "forge", "workspace.json")
    os.mkdir(path.getdirectory(file))
    local ok, err = os.writefile_ifnotequal(json.encode(document), file)
    if ok < 0 then
        error("forge: could not write " .. file .. ": " .. tostring(err))
    end
end

newaction {
    trigger = "forge-export",
    description = "Write build/forge/workspace.json (projects, kinds, targets, files, defines, includes)",
    onWorkspace = forge.export,
}

p.override(p.action, "call", function(base, name)
    base(name)
    if _OPTIONS["forge-export"] and name ~= "forge-export" then
        for wks in p.global.eachWorkspace() do
            forge.export(wks)
        end
    end
end)

local config_cache

local function config()
    if config_cache == nil then
        local file = path.join(_MAIN_SCRIPT_DIR, "build", "forge", "config.json")
        local text = io.readfile(file)
        if not text then
            error("forge: " .. file .. " is missing; generate the build through `forge build configure`")
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
