#include "oxpch.h"
#include "Oryx/Shaders/ShaderSourceProvider.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Core/Log.h"

namespace oryx
{

namespace
{

constexpr std::string_view MOUNT = "/Oryx/";

std::map<std::string, std::string>& embedded()
{
    static std::map<std::string, std::string> map;
    return map;
}

} // namespace

ShaderLanguage shader_language_for_path(std::string_view virtual_path)
{
    const size_t dot = virtual_path.rfind('.');
    if (dot != std::string_view::npos && virtual_path.substr(dot) == ".msl")
    {
        return ShaderLanguage::MSL;
    }
    throw Error("cannot infer a shader language from '" + std::string(virtual_path) + "'");
}

std::string shader_include_virtual_path(std::string_view include_name)
{
    return "/" + std::string(include_name);
}

void register_embedded_shader(std::string virtual_path, std::string text)
{
    embedded()[std::move(virtual_path)] = std::move(text);
}

std::vector<std::string> embedded_shader_paths()
{
    std::vector<std::string> paths;
    for (const std::pair<const std::string, std::string>& entry : embedded())
    {
        paths.push_back(entry.first);
    }
    return paths;
}

std::optional<ShaderSource> EmbeddedShaderSourceProvider::load(std::string_view virtual_path) const
{
    const std::map<std::string, std::string>::const_iterator it = embedded().find(std::string(virtual_path));
    if (it == embedded().end())
    {
        return std::nullopt;
    }
    return ShaderSource{ it->first, shader_language_for_path(virtual_path), it->second };
}

std::optional<ShaderSource> FileShaderSourceProvider::load(std::string_view virtual_path) const
{
    if (virtual_path.substr(0, MOUNT.size()) != MOUNT)
    {
        return std::nullopt;
    }
    const std::filesystem::path file = m_root / std::string(virtual_path.substr(MOUNT.size()));
    std::ifstream stream(file, std::ios::binary);
    if (!stream)
    {
        return std::nullopt;
    }
    std::ostringstream text;
    text << stream.rdbuf();
    return ShaderSource{ file.generic_string(), shader_language_for_path(virtual_path), text.str() };
}

std::optional<ShaderSource> LayeredShaderSourceProvider::load(std::string_view virtual_path) const
{
    if (std::optional<ShaderSource> source = m_primary.load(virtual_path))
    {
        return source;
    }
    std::optional<ShaderSource> source = m_fallback.load(virtual_path);
    if (source && m_warned.insert(std::string(virtual_path)).second)
    {
        OX_CORE_WARN("shader '{}' not found on disk; using the embedded copy", virtual_path);
    }
    return source;
}

} // namespace oryx
