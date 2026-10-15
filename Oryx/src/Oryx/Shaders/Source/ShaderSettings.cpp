#include "oxpch.h"
#include "Oryx/Shaders/Source/ShaderSettings.h"

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

} // namespace

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
