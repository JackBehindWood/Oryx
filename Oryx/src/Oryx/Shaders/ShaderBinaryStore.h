#pragma once

#include "Oryx/Shaders/ShaderCompiler.h"

namespace oryx
{

// What a persistent store holds: one compiled shader per ShaderHash, and the cooked map of an application's shader types.
enum class ShaderStoreKind : uint8_t
{
    Binary,
    Map
};

// Persistent derived data for compiled shaders, implemented by whoever owns storage (Assets, tests); Shaders never names where it lives.
// A missing, corrupt or foreign entry is a miss; write failures are the store's to report and never throw.
class IShaderBinaryStore
{
public:
    virtual ~IShaderBinaryStore() = default;

    [[nodiscard]] virtual bool read(ShaderStoreKind kind, uint64_t key, std::vector<uint8_t>& payload) const = 0;
    virtual void write(ShaderStoreKind kind, uint64_t key, const uint8_t* payload, size_t size) const = 0;
};

// Serialise/deserialise through the store; false on a miss or an unreadable payload.
[[nodiscard]] bool read_shader_output(const IShaderBinaryStore& store, ShaderHash hash, ShaderCompilerOutput& out);
void write_shader_output(const IShaderBinaryStore& store, ShaderHash hash, const ShaderCompilerOutput& output);

} // namespace oryx
