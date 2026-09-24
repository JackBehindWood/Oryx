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

newoption {
    trigger = "sanitize",
    description = "Build with AddressSanitizer + UndefinedBehaviorSanitizer (opt-in dev/CI tool; keeps the profile's optimize level so it still exercises Release/Dist codegen, forces debug symbols on for readable reports; the allocation census stays linked, so ASan's new/delete-mismatch check is inactive)",
}

-- Keeps the profile's optimize level on purpose: some bugs only reproduce with the optimizer (decision log, "Sanitizer builds").
function forge.sanitizers()
    if _OPTIONS["sanitize"] == nil then
        return
    end

    buildoptions { "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-fno-sanitize-recover=undefined" }
    linkoptions { "-fsanitize=address,undefined" }
    symbols "On"
end

-- Shared per-project defaults, so each project doesn't repeat language/dialect/runtime/output-dir lines.
function forge.project_defaults()
    language "C++"
    cppdialect "C++20"
    staticruntime "off"

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

    forge.sanitizers()
end

-- Generalizes the vendoring pattern (a header-only lib checked out as a git
-- submodule under <project>/vendor/<lib>/) so any project can vendor one
-- with a single line instead of hand-written includedirs/removefiles. By
-- default the header is assumed to sit directly under vendor/<lib>/ (the
-- common case for single-header libs). Pass `headerSubdir` for a submodule
-- whose own repo layout nests the real header one level down (e.g.
-- doctest: vendor/doctest/doctest/doctest.h -> forge.header_dependency("doctest", "doctest")).
function forge.header_dependency(libName, headerSubdir)
    local dir = "vendor/" .. libName
    if headerSubdir and headerSubdir ~= "" then
        dir = dir .. "/" .. headerSubdir
    end
    includedirs { dir }
    removefiles { "vendor/" .. libName .. "/**" }
end

-- Whole-archive linking, because self-registering objects have no other referenced symbol and a
-- plain `links {lib}` would drop them. `extra` are additional plain links (e.g. a library `lib`
-- pulls in transitively) that don't themselves need whole-archiving.
function forge.whole_archive(lib, extra)
    -- A project link (not just the linkoptions below) makes the executable relink whenever the library changes.
    links { lib }
    links(extra or {})

    filter "system:windows"
        linkoptions { "/WHOLEARCHIVE:" .. lib .. ".lib" }

    filter "system:macosx"
        linkoptions { "-force_load \"%{wks.location}/bin/" .. outputdir .. "/" .. lib .. "/lib" .. lib .. ".a\"" }

    filter "system:linux"
        links { "pthread" }
        linkoptions { "-Wl,--whole-archive", "%{wks.location}/bin/" .. outputdir .. "/" .. lib .. "/lib" .. lib .. ".a", "-Wl,--no-whole-archive" }

    filter {}
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
