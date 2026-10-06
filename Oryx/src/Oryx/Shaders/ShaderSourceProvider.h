#pragma once

#include "Oryx/Shaders/ShaderSource.h"

namespace oryx
{

// Shader sources are addressed by virtual path ("/Oryx/Builtin/Solid.msl"); the provider decides where the bytes come from.
class IShaderSourceProvider
{
public:
    virtual ~IShaderSourceProvider() = default;

    // Returns nullopt when the path is unknown.
    [[nodiscard]] virtual std::optional<ShaderSource> load(std::string_view virtual_path) const = 0;
};

// The language a virtual path's extension names; throws Error for an unknown extension.
[[nodiscard]] ShaderLanguage shader_language_for_path(std::string_view virtual_path);

// An include name such as "Oryx/Common.msl" lives at the virtual path "/Oryx/Common.msl".
[[nodiscard]] std::string shader_include_virtual_path(std::string_view include_name);

// Sources compiled into the binary by tools/embed_shaders.py, registered by static registrars.
void register_embedded_shader(std::string virtual_path, std::string text);

struct EmbeddedShaderRegistrar
{
    EmbeddedShaderRegistrar(const char* virtual_path, const char* text, size_t size)
    {
        register_embedded_shader(virtual_path, std::string(text, size));
    }
};

[[nodiscard]] std::vector<std::string> embedded_shader_paths();

class EmbeddedShaderSourceProvider final : public IShaderSourceProvider
{
public:
    [[nodiscard]] std::optional<ShaderSource> load(std::string_view virtual_path) const override;
};

// Maps the "/Oryx/" mount onto a directory of loose files; read on every load so edits are seen on reload.
class FileShaderSourceProvider final : public IShaderSourceProvider
{
public:
    explicit FileShaderSourceProvider(std::filesystem::path root)
        : m_root(std::move(root))
    {
    }

    [[nodiscard]] std::optional<ShaderSource> load(std::string_view virtual_path) const override;
    [[nodiscard]] const std::filesystem::path& root() const { return m_root; }

private:
    std::filesystem::path m_root;
};

// Tries `primary`, then `fallback`; logs a one-time warning per path that needed the fallback.
class LayeredShaderSourceProvider final : public IShaderSourceProvider
{
public:
    LayeredShaderSourceProvider(const IShaderSourceProvider& primary, const IShaderSourceProvider& fallback)
        : m_primary(primary)
        , m_fallback(fallback)
    {
    }

    [[nodiscard]] std::optional<ShaderSource> load(std::string_view virtual_path) const override;

private:
    const IShaderSourceProvider& m_primary;
    const IShaderSourceProvider& m_fallback;
    mutable std::set<std::string> m_warned;
};

} // namespace oryx
