#pragma once

#include "Oryx/Core/Settings.h"
#include "Oryx/Shaders/ShaderSourceProvider.h"

namespace oryx
{

enum class ShaderSourceMode : uint8_t
{
    Auto,
    File,
    Embedded,
    FileThenEmbedded
};

// The `shaders:` section. Auto reads loose files in Debug and Release, warning once per path that falls back to the embedded copy (test and
// application shader types live only there), and the embedded copy alone in Dist. `file` is strict: a missing file is an error.
struct ShaderSettings
{
    // Directory the "/Oryx/" mount maps to; a relative root is relative to the settings file.
    std::filesystem::path root;
    ShaderSourceMode source_mode = ShaderSourceMode::Auto;
};

void read_settings(ShaderSettings& settings, const SettingsNode& node);

// Auto resolved against the build configuration.
[[nodiscard]] ShaderSourceMode effective_source_mode(const ShaderSettings& settings);

// Owns the file/layered providers a mode needs and exposes the one the renderer should use.
class ShaderSourceResolver
{
public:
    explicit ShaderSourceResolver(const ShaderSettings& settings);

    [[nodiscard]] const IShaderSourceProvider& provider() const { return *m_active; }
    [[nodiscard]] ShaderSourceMode mode() const { return m_mode; }

private:
    ShaderSourceMode m_mode;
    EmbeddedShaderSourceProvider m_embedded;
    FileShaderSourceProvider m_file;
    LayeredShaderSourceProvider m_layered;
    const IShaderSourceProvider* m_active = nullptr;
};

} // namespace oryx
