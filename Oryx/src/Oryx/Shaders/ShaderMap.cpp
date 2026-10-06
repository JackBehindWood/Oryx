#include "oxpch.h"
#include "Oryx/Shaders/ShaderMap.h"

#include "Oryx/Core/Fnv.h"

namespace oryx
{

namespace
{

constexpr uint32_t MAGIC = 0x4F58534D;
constexpr uint32_t FORMAT_VERSION = 1;
constexpr uint32_t MAX_ENTRIES = 1u << 20;

template<typename T>
void put(std::vector<uint8_t>& out, const T& value)
{
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
    out.insert(out.end(), bytes, bytes + sizeof(T));
}

template<typename T>
bool take(const uint8_t* data, size_t size, size_t& offset, T& value)
{
    if (size - offset < sizeof(T))
    {
        return false;
    }
    std::memcpy(&value, data + offset, sizeof(T));
    offset += sizeof(T);
    return true;
}

} // namespace

const ShaderMapEntry* ShaderMap::find(std::string_view type, uint32_t permutation) const
{
    for (const ShaderMapEntry& entry : entries)
    {
        if (entry.permutation == permutation && entry.type == type)
        {
            return &entry;
        }
    }
    return nullptr;
}

uint64_t shader_map_id(const std::vector<ShaderType>& types)
{
    std::vector<const ShaderType*> ordered;
    for (const ShaderType& type : types)
    {
        ordered.push_back(&type);
    }
    std::sort(ordered.begin(), ordered.end(), [](const ShaderType* a, const ShaderType* b) { return std::strcmp(a->name, b->name) < 0; });

    Fnv1a hash;
    hash.mix_value(ordered.size());
    for (const ShaderType* type : ordered)
    {
        hash.mix_string(type->name);
        hash.mix_string(type->source);
        hash.mix_string(type->entry_point);
        hash.mix_value(static_cast<uint64_t>(type->stage));
        for (uint32_t permutation = 0; permutation < SHADER_MAX_PERMUTATIONS; ++permutation)
        {
            if (!type->should_compile(permutation))
            {
                continue;
            }
            hash.mix_value(permutation);
            std::vector<ShaderDefine> defines = type->defines_for(permutation);
            std::sort(defines.begin(), defines.end(), [](const ShaderDefine& a, const ShaderDefine& b) { return a.name < b.name; });
            for (const ShaderDefine& define : defines)
            {
                hash.mix_string(define.name);
                hash.mix_string(define.value);
            }
        }
    }
    return hash.value();
}

std::vector<uint8_t> serialise_shader_map(const ShaderMap& map)
{
    std::vector<uint8_t> out;
    put(out, MAGIC);
    put(out, FORMAT_VERSION);
    put(out, map.id);
    put(out, static_cast<uint32_t>(map.entries.size()));
    for (const ShaderMapEntry& entry : map.entries)
    {
        put(out, static_cast<uint32_t>(entry.type.size()));
        out.insert(out.end(), entry.type.begin(), entry.type.end());
        put(out, entry.permutation);
        put(out, entry.hash);
    }
    return out;
}

bool deserialise_shader_map(const uint8_t* data, size_t size, ShaderMap& out)
{
    size_t offset = 0;
    uint32_t magic = 0;
    uint32_t version = 0;
    uint32_t count = 0;
    ShaderMap result;
    if (!take(data, size, offset, magic) || !take(data, size, offset, version) || magic != MAGIC || version != FORMAT_VERSION)
    {
        return false;
    }
    if (!take(data, size, offset, result.id) || !take(data, size, offset, count) || count > MAX_ENTRIES)
    {
        return false;
    }
    result.entries.resize(count);
    for (ShaderMapEntry& entry : result.entries)
    {
        uint32_t length = 0;
        if (!take(data, size, offset, length) || length > size - offset)
        {
            return false;
        }
        entry.type.assign(reinterpret_cast<const char*>(data + offset), length);
        offset += length;
        if (!take(data, size, offset, entry.permutation) || !take(data, size, offset, entry.hash))
        {
            return false;
        }
    }
    if (offset != size)
    {
        return false;
    }
    out = std::move(result);
    return true;
}

void write_shader_map(const IShaderBinaryStore& store, const ShaderMap& map)
{
    const std::vector<uint8_t> payload = serialise_shader_map(map);
    store.write(ShaderStoreKind::Map, map.id, payload.data(), payload.size());
}

bool read_shader_map(const IShaderBinaryStore& store, uint64_t id, ShaderMap& out)
{
    std::vector<uint8_t> payload;
    ShaderMap map;
    if (!store.read(ShaderStoreKind::Map, id, payload) || !deserialise_shader_map(payload.data(), payload.size(), map) || map.id != id)
    {
        return false;
    }
    out = std::move(map);
    return true;
}

} // namespace oryx
