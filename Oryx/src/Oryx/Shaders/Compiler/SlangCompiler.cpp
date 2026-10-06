#include "oxpch.h"
#include "Oryx/Shaders/Compiler/SlangCompiler.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Shaders/Compiler/ShaderInclude.h"
#include "Oryx/Shaders/Source/ShaderSourceProvider.h"

#include <yaml-cpp/yaml.h>

#include <fcntl.h>
#include <regex>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __APPLE__
#include <crt_externs.h>
#else
extern char** environ;
#endif

namespace oryx
{

namespace
{

char** process_environment()
{
#ifdef __APPLE__
    return *_NSGetEnviron();
#else
    return environ;
#endif
}

class ScratchDirectory
{
public:
    ScratchDirectory()
    {
        std::string pattern = (std::filesystem::temp_directory_path() / "oryx-slang-XXXXXX").string();
        if (mkdtemp(pattern.data()) == nullptr)
        {
            throw Error("cannot create a temporary directory for slangc");
        }
        m_path = pattern;
    }

    ~ScratchDirectory()
    {
        std::error_code ignored;
        std::filesystem::remove_all(m_path, ignored);
    }

    ScratchDirectory(const ScratchDirectory&) = delete;
    ScratchDirectory& operator=(const ScratchDirectory&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const { return m_path; }

private:
    std::filesystem::path m_path;
};

void write_file(const std::filesystem::path& file, const std::string& text)
{
    std::filesystem::create_directories(file.parent_path());
    std::ofstream stream(file, std::ios::binary);
    stream << text;
    if (!stream)
    {
        throw Error("cannot write '" + file.string() + "'");
    }
}

std::string read_file(const std::filesystem::path& file)
{
    std::ifstream stream(file, std::ios::binary);
    std::ostringstream text;
    text << stream.rdbuf();
    return text.str();
}

int run_process(const std::vector<std::string>& args, const std::filesystem::path& log)
{
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, log.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO, STDERR_FILENO);

    std::vector<char*> argv;
    for (const std::string& arg : args)
    {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);

    pid_t pid = 0;
    const int spawned = args[0].find('/') == std::string::npos ? posix_spawnp(&pid, args[0].c_str(), &actions, nullptr, argv.data(), process_environment())
                                                                : posix_spawn(&pid, args[0].c_str(), &actions, nullptr, argv.data(), process_environment());
    posix_spawn_file_actions_destroy(&actions);
    if (spawned != 0)
    {
        throw Error("cannot run slangc at '" + args[0] + "': " + std::strerror(spawned), "run `forge deps sync` to fetch it, or set OX_SLANGC or shaders.slangc to a slangc binary");
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0)
    {
        if (errno != EINTR)
        {
            throw Error("waiting for slangc failed");
        }
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

const char* slang_stage_name(ShaderStage stage)
{
    switch (stage)
    {
    case ShaderStage::Vertex: return "vertex";
    case ShaderStage::Pixel: return "fragment";
    case ShaderStage::Compute: return "compute";
    case ShaderStage::TessControl: return "hull";
    case ShaderStage::TessEval: return "domain";
    }
    return "vertex";
}

struct Diagnostics
{
    std::vector<std::string> errors;
    std::vector<std::string> others;
};

Diagnostics translate_diagnostics(const std::string& log, const std::filesystem::path& scratch, const std::string& source_name)
{
    static const std::regex header(R"(^(error|warning)(?:\[\w+\])?: (.*)$)");
    static const std::regex location(R"(^\s*--> (.*):(\d+):\d+\s*$)");
    Diagnostics result;
    std::istringstream stream(log);
    std::string line;
    std::string pending_kind;
    std::string pending_text;
    const auto flush = [&](const std::string& where) {
        if (!pending_kind.empty())
        {
            (pending_kind == "error" ? result.errors : result.others).push_back(where + pending_text);
            pending_kind.clear();
        }
    };
    while (std::getline(stream, line))
    {
        std::smatch match;
        if (std::regex_match(line, match, header))
        {
            flush(source_name + ": ");
            pending_kind = match[1].str();
            pending_text = match[2].str();
        }
        else if (!pending_kind.empty() && std::regex_match(line, match, location))
        {
            const std::filesystem::path file = match[1].str();
            std::string name = source_name;
            if (file != scratch / "main.slang" && file != "main.slang")
            {
                const std::filesystem::path relative = file.lexically_relative(scratch);
                name = relative.empty() || *relative.begin() == ".." ? file.string() : relative.generic_string();
            }
            flush(name + ":" + match[2].str() + ": ");
        }
    }
    flush(source_name + ": ");
    return result;
}

std::string join(const std::vector<std::string>& lines)
{
    std::string text;
    for (const std::string& line : lines)
    {
        text += (text.empty() ? "" : "\n") + line;
    }
    return text;
}

ShaderScalar scalar_of(const std::string& name)
{
    if (name == "float32") return ShaderScalar::Float;
    if (name == "float16") return ShaderScalar::Half;
    if (name == "int32") return ShaderScalar::Int;
    if (name == "uint32") return ShaderScalar::UInt;
    if (name == "bool") return ShaderScalar::Bool;
    throw Error("shader uses the unsupported scalar type '" + name + "'");
}

ShaderDataType data_type_of(const YAML::Node& type)
{
    const std::string kind = type["kind"].as<std::string>();
    ShaderDataType result;
    if (kind == "scalar")
    {
        result.scalar = scalar_of(type["scalarType"].as<std::string>());
    }
    else if (kind == "vector")
    {
        result.scalar = scalar_of(type["elementType"]["scalarType"].as<std::string>());
        result.rows = type["elementCount"].as<uint8_t>();
    }
    else if (kind == "matrix")
    {
        result.scalar = scalar_of(type["elementType"]["scalarType"].as<std::string>());
        result.rows = static_cast<uint8_t>(type["rowCount"].as<uint32_t>());
        result.columns = static_cast<uint8_t>(type["columnCount"].as<uint32_t>());
    }
    else
    {
        throw Error("shader interface uses the unsupported type kind '" + kind + "'");
    }
    return result;
}

uint32_t uniform_size(const YAML::Node& type)
{
    for (const YAML::Node& size : type["sizes"])
    {
        if (size["kind"].as<std::string>() == "uniform")
        {
            return size["value"].as<uint32_t>();
        }
    }
    return 0;
}

bool is_builtin_semantic(const YAML::Node& node)
{
    if (!node["semanticName"])
    {
        return false;
    }
    const std::string semantic = node["semanticName"].as<std::string>();
    return semantic.rfind("SV_", 0) == 0 && semantic != "SV_TARGET";
}

void collect_varying(const YAML::Node& node, const char* kind, std::vector<ShaderStageVariable>& out)
{
    const YAML::Node binding = node["binding"];
    const bool bound = binding && binding["kind"].as<std::string>() == kind;
    const uint32_t base = bound ? binding["index"].as<uint32_t>() : 0;
    const YAML::Node type = node["type"];
    if (type["kind"].as<std::string>() != "struct")
    {
        ShaderStageVariable variable;
        variable.name = node["name"] ? node["name"].as<std::string>() : "result";
        variable.type = data_type_of(type);
        variable.location = base;
        variable.builtin = is_builtin_semantic(node);
        out.push_back(variable);
        return;
    }
    for (const YAML::Node& field : type["fields"])
    {
        ShaderStageVariable variable;
        variable.name = field["name"].as<std::string>();
        variable.type = data_type_of(field["type"]);
        variable.builtin = is_builtin_semantic(field);
        const YAML::Node field_binding = field["binding"];
        if (field_binding && field_binding["kind"].as<std::string>() == kind)
        {
            variable.location = base + field_binding["index"].as<uint32_t>();
        }
        out.push_back(variable);
    }
}

ShaderBinding binding_of(const YAML::Node& parameter)
{
    ShaderBinding result;
    result.name = parameter["name"].as<std::string>();
    const YAML::Node binding = parameter["binding"];
    const std::string kind = binding["kind"].as<std::string>();
    result.slot = binding["index"].as<uint32_t>(0);
    YAML::Node type = parameter["type"];
    if (type["kind"].as<std::string>() == "array")
    {
        result.array_count = type["elementCount"].as<uint32_t>();
        type = type["elementType"];
    }
    const std::string type_kind = type["kind"].as<std::string>();

    if (kind == "constantBuffer")
    {
        const YAML::Node element = type["elementType"];
        if (type_kind != "constantBuffer" || element["kind"].as<std::string>() != "struct")
        {
            throw Error("constant buffer '" + result.name + "' must hold a struct");
        }
        result.kind = ShaderBindingKind::Constants;
        result.size = uniform_size(element);
        for (const YAML::Node& field : element["fields"])
        {
            ShaderStructMember member;
            member.name = field["name"].as<std::string>();
            member.type = data_type_of(field["type"]);
            member.offset = field["binding"]["offset"].as<uint32_t>();
            member.size = field["binding"]["size"].as<uint32_t>();
            result.members.push_back(member);
        }
    }
    else if (kind == "samplerState")
    {
        result.kind = ShaderBindingKind::Sampler;
    }
    else if (kind == "shaderResource" || kind == "unorderedAccess")
    {
        const bool writable = kind == "unorderedAccess";
        const std::string shape = type_kind == "resource" ? type["baseShape"].as<std::string>() : type_kind;
        if (shape == "structuredBuffer" || shape == "byteAddressBuffer")
        {
            result.kind = ShaderBindingKind::StorageBuffer;
        }
        else
        {
            result.kind = writable ? ShaderBindingKind::StorageTexture : ShaderBindingKind::SampledTexture;
            if (shape == "texture2D") result.texture_dimension = type["multisample"].as<bool>(false) ? ShaderTextureDimension::Tex2DMultisample : ShaderTextureDimension::Tex2D;
            else if (shape == "texture2DArray") result.texture_dimension = ShaderTextureDimension::Tex2DArray;
            else if (shape == "textureCube") result.texture_dimension = ShaderTextureDimension::Cube;
            else if (shape == "texture3D") result.texture_dimension = ShaderTextureDimension::Tex3D;
            else throw Error("parameter '" + result.name + "' has the unsupported resource shape '" + shape + "'");
            const YAML::Node element = type["resultType"];
            const ShaderScalar scalar = element ? data_type_of(element).scalar : ShaderScalar::Float;
            result.texture_data_type = scalar == ShaderScalar::Int ? ShaderTextureData::Int : scalar == ShaderScalar::UInt ? ShaderTextureData::UInt : ShaderTextureData::Float;
        }
    }
    else
    {
        throw Error("parameter '" + result.name + "' is a loose '" + kind + "' value; declare it inside a ConstantBuffer");
    }
    return result;
}

} // namespace

std::filesystem::path slangc_path(const SlangCompilerOptions& options)
{
    if (const char* env = std::getenv("OX_SLANGC"); env != nullptr && *env != '\0')
    {
        return env;
    }
    if (!options.slangc.empty())
    {
        return options.slangc;
    }
#ifdef OX_SLANGC_PATH
    return OX_SLANGC_PATH;
#else
    return "slangc";
#endif
}

ShaderReflection parse_slang_reflection(const std::string& json, const std::string& entry_point, ShaderStage stage)
{
    YAML::Node root;
    try
    {
        root = YAML::Load(json);
    }
    catch (const YAML::Exception& error)
    {
        throw Error(std::string("slangc reflection is not valid JSON: ") + error.what());
    }

    try
    {
        const YAML::Node entries = root["entryPoints"];
        if (!entries || entries.size() != 1)
        {
            throw Error("slangc reflected " + std::to_string(entries ? entries.size() : 0) + " entry points; expected exactly '" + entry_point + "'");
        }
        const YAML::Node entry = entries[0];
        if (entry["name"].as<std::string>() != entry_point)
        {
            throw Error("slangc reflected entry point '" + entry["name"].as<std::string>() + "', expected '" + entry_point + "'");
        }
        if (entry["stage"].as<std::string>() != slang_stage_name(stage))
        {
            throw Error("entry point '" + entry_point + "' is a " + entry["stage"].as<std::string>() + " shader, registered as " + shader_stage_name(stage));
        }

        ShaderReflection reflection;
        reflection.entry_point = entry_point;
        reflection.stage = stage;
        for (const YAML::Node& parameter : entry["parameters"])
        {
            collect_varying(parameter, "varyingInput", reflection.inputs);
        }
        if (entry["result"])
        {
            collect_varying(entry["result"], "varyingOutput", reflection.outputs);
        }

        std::set<std::string> used;
        for (const YAML::Node& binding : entry["bindings"])
        {
            if (binding["binding"]["used"].as<uint32_t>(1) != 0)
            {
                used.insert(binding["name"].as<std::string>());
            }
        }
        for (const YAML::Node& parameter : root["parameters"])
        {
            if (used.count(parameter["name"].as<std::string>()) != 0)
            {
                reflection.parameters.push_back(binding_of(parameter));
            }
        }
        if (const YAML::Node size = entry["threadGroupSize"])
        {
            for (uint32_t axis = 0; axis < 3 && axis < size.size(); ++axis)
            {
                reflection.thread_group_size[axis] = size[axis].as<uint32_t>();
            }
        }
        return reflection;
    }
    catch (const YAML::Exception& error)
    {
        throw Error(std::string("unexpected slangc reflection layout: ") + error.what());
    }
}

std::vector<std::string> SlangCompiler::dependencies(const ShaderSource& source) const
{
    return shader_include_closure(source.text, [](const std::string&) { return nullptr; });
}

ShaderCompilerOutput SlangCompiler::compile(const ShaderCompilerInput& input) const
{
    if (input.target != ShaderBinaryFormat::MslSource)
    {
        throw Error(input.source.name + ": the Slang compiler only emits MSL source");
    }

    const ScratchDirectory scratch;
    const std::filesystem::path main_file = scratch.path() / "main.slang";
    write_file(main_file, input.source.text);

    std::vector<std::string> pending = dependencies(input.source);
    std::set<std::string> written;
    while (!pending.empty())
    {
        const std::string name = pending.back();
        pending.pop_back();
        if (!written.insert(name).second)
        {
            continue;
        }
        const std::map<std::string, std::string>::const_iterator local = input.includes.find(name);
        const std::string* text = local != input.includes.end() ? &local->second : find_shader_include(name);
        if (text == nullptr)
        {
            throw Error(input.source.name + ": include '" + name + "' not found");
        }
        write_file(scratch.path() / name, *text);
        ShaderSource nested;
        nested.text = *text;
        for (const std::string& next : dependencies(nested))
        {
            pending.push_back(next);
        }
    }

    const std::filesystem::path output_file = scratch.path() / "out.metal";
    const std::filesystem::path reflection_file = scratch.path() / "out.json";
    const std::filesystem::path log_file = scratch.path() / "slangc.log";
    std::vector<std::string> args = { slangc_path(m_options).string(), main_file.string(), "-target", "metal", "-entry", input.entry_point, "-stage", slang_stage_name(input.stage),
                                      "-matrix-layout-column-major", "-line-directive-mode", "none", "-I", scratch.path().string(), "-DOX_SLANG=1",
                                      "-o", output_file.string(), "-reflection-json", reflection_file.string() };
    for (const ShaderDefine& define : input.defines)
    {
        args.push_back("-D" + define.name + (define.value.empty() ? "" : "=" + define.value));
    }

    const int status = run_process(args, log_file);
    const Diagnostics diagnostics = translate_diagnostics(read_file(log_file), scratch.path(), input.source.name);
    if (status != 0)
    {
        if (diagnostics.errors.empty())
        {
            throw Error(input.source.name + ": slangc failed (exit " + std::to_string(status) + ")", join(diagnostics.others));
        }
        std::vector<std::string> lines;
        for (const std::string& note : diagnostics.others)
        {
            if (note.find("stage mismatch") != std::string::npos)
            {
                lines.push_back(note);
            }
        }
        lines.insert(lines.end(), diagnostics.errors.begin(), diagnostics.errors.end());
        throw Error(join(lines));
    }

    for (const std::string& note : diagnostics.others)
    {
        if (note.find("stage mismatch") != std::string::npos)
        {
            throw Error(note);
        }
    }

    ShaderCompilerOutput output;
    const std::string msl = read_file(output_file);
    output.binary.assign(msl.begin(), msl.end());
    output.format = ShaderBinaryFormat::MslSource;
    output.language = ShaderLanguage::Slang;
    try
    {
        output.reflection = parse_slang_reflection(read_file(reflection_file), input.entry_point, input.stage);
    }
    catch (const Error& error)
    {
        throw Error(input.source.name + ": " + error.what());
    }
    output.diagnostics = diagnostics.others;
    return output;
}

} // namespace oryx
