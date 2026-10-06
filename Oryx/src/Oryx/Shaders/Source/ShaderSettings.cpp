#include "oxpch.h"
#include "Oryx/Shaders/Source/ShaderSettings.h"

#include "Oryx/Core/Error.h"

namespace oryx
{

namespace
{

std::filesystem::path default_root()
{
#ifdef OX_SHADER_ROOT
    return OX_SHADER_ROOT;
#else
    return "shaders";
#endif
}

ShaderSourceMode parse_mode(const std::string& text)
{
    if (text == "auto") return ShaderSourceMode::Auto;
    if (text == "file") return ShaderSourceMode::File;
    if (text == "embedded") return ShaderSourceMode::Embedded;
    if (text == "file_then_embedded") return ShaderSourceMode::FileThenEmbedded;
    if (text == "cooked") return ShaderSourceMode::Cooked;
    throw SettingsError("shaders.source_mode must be auto, file, embedded, file_then_embedded or cooked, got '" + text + "'");
}

} // namespace

void read_settings(ShaderSettings& settings, const SettingsNode& node)
{
    settings.root = node.path("root", settings.root);
    settings.slangc = node.path("slangc", settings.slangc);
    if (node.has("source_mode"))
    {
        settings.source_mode = parse_mode(node.string("source_mode"));
    }
}

ShaderSourceMode effective_source_mode(const ShaderSettings& settings)
{
    if (settings.source_mode != ShaderSourceMode::Auto)
    {
        return settings.source_mode;
    }
#ifdef OX_DIST
    return ShaderSourceMode::Cooked;
#else
    return ShaderSourceMode::FileThenEmbedded;
#endif
}

ShaderSourceResolver::ShaderSourceResolver(const ShaderSettings& settings)
    : m_mode(effective_source_mode(settings))
    , m_file(settings.root.empty() ? default_root() : settings.root)
    , m_layered(m_file, m_embedded)
{
    switch (m_mode)
    {
    case ShaderSourceMode::File: m_active = &m_file; break;
    case ShaderSourceMode::FileThenEmbedded: m_active = &m_layered; break;
    default: m_active = &m_embedded; break;
    }
}

} // namespace oryx

OX_REGISTER_SETTINGS(oryx::ShaderSettings, "shaders")
