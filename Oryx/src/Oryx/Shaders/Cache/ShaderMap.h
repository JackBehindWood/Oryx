#pragma once

#include "Oryx/Shaders/Cache/ShaderBinaryStore.h"
#include "Oryx/Shaders/ShaderType.h"

namespace oryx
{

struct ShaderMapEntry
{
    std::string type;
    uint32_t permutation = 0;
    ShaderHash hash = 0;
};

// The cooked manifest: which compiled shader each (shader type, permutation) uses. It lets a build with no sources find its binaries,
// since their ShaderHash can only be computed from source. `id` identifies the set of registered shader types it was cooked for.
struct ShaderMap
{
    uint64_t id = 0;
    std::vector<ShaderMapEntry> entries;

    [[nodiscard]] const ShaderMapEntry* find(std::string_view type, uint32_t permutation) const;
};

// FNV over every registered type's name, source path, entry, stage and the defines of each permutation it compiles, in name order.
[[nodiscard]] uint64_t shader_map_id(const std::vector<ShaderType>& types);

[[nodiscard]] std::vector<uint8_t> serialise_shader_map(const ShaderMap& map);
[[nodiscard]] bool deserialise_shader_map(const uint8_t* data, size_t size, ShaderMap& out);

void write_shader_map(const IShaderBinaryStore& store, const ShaderMap& map);
// Reads the map cooked for `id`; false when absent, unreadable or cooked for a different type set.
[[nodiscard]] bool read_shader_map(const IShaderBinaryStore& store, uint64_t id, ShaderMap& out);

} // namespace oryx
