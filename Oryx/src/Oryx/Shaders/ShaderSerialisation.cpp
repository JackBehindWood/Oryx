#include "oxpch.h"
#include "Oryx/Shaders/ShaderSerialisation.h"

#include "Oryx/Shaders/ShaderBinaryStore.h"

namespace oryx
{

namespace
{

constexpr uint32_t MAGIC = 0x4F585348;
constexpr uint32_t FORMAT_VERSION = 1;
constexpr uint32_t MAX_BINARY_FORMAT = static_cast<uint32_t>(ShaderBinaryFormat::MetalLib);
constexpr uint32_t MAX_ELEMENTS = 1u << 16;

class Writer
{
public:
    void u8(uint8_t value) { m_bytes.push_back(value); }
    void u32(uint32_t value) { raw(&value, sizeof(value)); }
    void str(const std::string& text)
    {
        u32(static_cast<uint32_t>(text.size()));
        raw(text.data(), text.size());
    }
    void raw(const void* data, size_t size)
    {
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        m_bytes.insert(m_bytes.end(), bytes, bytes + size);
    }
    [[nodiscard]] std::vector<uint8_t> take() { return std::move(m_bytes); }

private:
    std::vector<uint8_t> m_bytes;
};

// Every read is bounds-checked; one failure makes the whole payload a miss.
class Reader
{
public:
    Reader(const uint8_t* data, size_t size)
        : m_data(data)
        , m_size(size)
    {
    }

    bool u8(uint8_t& out) { return raw(&out, 1); }
    bool u32(uint32_t& out) { return raw(&out, sizeof(out)); }
    bool str(std::string& out)
    {
        uint32_t length = 0;
        if (!u32(length) || length > m_size - m_offset)
        {
            return false;
        }
        out.assign(reinterpret_cast<const char*>(m_data + m_offset), length);
        m_offset += length;
        return true;
    }
    bool bytes(std::vector<uint8_t>& out)
    {
        uint32_t length = 0;
        if (!u32(length) || length > m_size - m_offset)
        {
            return false;
        }
        out.assign(m_data + m_offset, m_data + m_offset + length);
        m_offset += length;
        return true;
    }
    bool count(uint32_t& out) { return u32(out) && out <= MAX_ELEMENTS; }
    bool raw(void* out, size_t size)
    {
        if (size > m_size - m_offset)
        {
            return false;
        }
        std::memcpy(out, m_data + m_offset, size);
        m_offset += size;
        return true;
    }
    [[nodiscard]] bool at_end() const { return m_offset == m_size; }

private:
    const uint8_t* m_data;
    size_t m_size;
    size_t m_offset = 0;
};

void write_data_type(Writer& w, const ShaderDataType& type)
{
    w.u8(static_cast<uint8_t>(type.scalar));
    w.u8(type.rows);
    w.u8(type.columns);
}

bool read_data_type(Reader& r, ShaderDataType& type)
{
    uint8_t scalar = 0;
    if (!r.u8(scalar) || !r.u8(type.rows) || !r.u8(type.columns) || scalar > static_cast<uint8_t>(ShaderScalar::Bool))
    {
        return false;
    }
    type.scalar = static_cast<ShaderScalar>(scalar);
    return true;
}

void write_variables(Writer& w, const std::vector<ShaderStageVariable>& variables)
{
    w.u32(static_cast<uint32_t>(variables.size()));
    for (const ShaderStageVariable& variable : variables)
    {
        w.str(variable.name);
        w.u32(variable.location);
        write_data_type(w, variable.type);
        w.u8(variable.builtin ? 1 : 0);
    }
}

bool read_variables(Reader& r, std::vector<ShaderStageVariable>& variables)
{
    uint32_t count = 0;
    if (!r.count(count))
    {
        return false;
    }
    variables.resize(count);
    for (ShaderStageVariable& variable : variables)
    {
        uint8_t builtin = 0;
        if (!r.str(variable.name) || !r.u32(variable.location) || !read_data_type(r, variable.type) || !r.u8(builtin))
        {
            return false;
        }
        variable.builtin = builtin != 0;
    }
    return true;
}

void write_binding(Writer& w, const ShaderBinding& binding)
{
    w.str(binding.name);
    w.u8(static_cast<uint8_t>(binding.kind));
    w.u32(binding.slot);
    w.u32(binding.array_count);
    w.u32(binding.size);
    w.u8(static_cast<uint8_t>(binding.texture_data_type));
    w.u8(static_cast<uint8_t>(binding.texture_dimension));
    w.u32(static_cast<uint32_t>(binding.members.size()));
    for (const ShaderStructMember& member : binding.members)
    {
        w.str(member.name);
        write_data_type(w, member.type);
        w.u32(member.offset);
        w.u32(member.size);
    }
}

bool read_binding(Reader& r, ShaderBinding& binding)
{
    uint8_t kind = 0;
    uint8_t data = 0;
    uint8_t dimension = 0;
    uint32_t members = 0;
    if (!r.str(binding.name) || !r.u8(kind) || !r.u32(binding.slot) || !r.u32(binding.array_count) || !r.u32(binding.size) || !r.u8(data) || !r.u8(dimension) || !r.count(members))
    {
        return false;
    }
    if (kind > static_cast<uint8_t>(ShaderBindingKind::Sampler) || data > static_cast<uint8_t>(ShaderTextureData::Depth) || dimension > static_cast<uint8_t>(ShaderTextureDimension::Tex2DMultisample))
    {
        return false;
    }
    binding.kind = static_cast<ShaderBindingKind>(kind);
    binding.texture_data_type = static_cast<ShaderTextureData>(data);
    binding.texture_dimension = static_cast<ShaderTextureDimension>(dimension);
    binding.members.resize(members);
    for (ShaderStructMember& member : binding.members)
    {
        if (!r.str(member.name) || !read_data_type(r, member.type) || !r.u32(member.offset) || !r.u32(member.size))
        {
            return false;
        }
    }
    return true;
}

} // namespace

std::vector<uint8_t> serialise_shader_output(const ShaderCompilerOutput& output)
{
    Writer w;
    w.u32(MAGIC);
    w.u32(FORMAT_VERSION);
    w.u8(static_cast<uint8_t>(output.format));
    w.u8(static_cast<uint8_t>(output.language));
    w.str(output.compiler_id);
    w.u32(output.compiler_version);
    w.u32(static_cast<uint32_t>(output.binary.size()));
    w.raw(output.binary.data(), output.binary.size());
    w.u32(static_cast<uint32_t>(output.diagnostics.size()));
    for (const std::string& diagnostic : output.diagnostics)
    {
        w.str(diagnostic);
    }

    const ShaderReflection& reflection = output.reflection;
    w.str(reflection.entry_point);
    w.u8(static_cast<uint8_t>(reflection.stage));
    write_variables(w, reflection.inputs);
    write_variables(w, reflection.outputs);
    w.u32(static_cast<uint32_t>(reflection.parameters.size()));
    for (const ShaderBinding& binding : reflection.parameters)
    {
        write_binding(w, binding);
    }
    for (uint32_t extent : reflection.thread_group_size)
    {
        w.u32(extent);
    }
    return w.take();
}

bool deserialise_shader_output(const uint8_t* data, size_t size, ShaderCompilerOutput& out)
{
    Reader r(data, size);
    uint32_t magic = 0;
    uint32_t version = 0;
    uint8_t format = 0;
    uint8_t language = 0;
    ShaderCompilerOutput result;
    if (!r.u32(magic) || !r.u32(version) || magic != MAGIC || version != FORMAT_VERSION)
    {
        return false;
    }
    if (!r.u8(format) || !r.u8(language) || format > MAX_BINARY_FORMAT || language >= SHADER_LANGUAGE_COUNT)
    {
        return false;
    }
    result.format = static_cast<ShaderBinaryFormat>(format);
    result.language = static_cast<ShaderLanguage>(language);
    uint32_t diagnostics = 0;
    if (!r.str(result.compiler_id) || !r.u32(result.compiler_version) || !r.bytes(result.binary) || !r.count(diagnostics))
    {
        return false;
    }
    result.diagnostics.resize(diagnostics);
    for (std::string& diagnostic : result.diagnostics)
    {
        if (!r.str(diagnostic))
        {
            return false;
        }
    }

    ShaderReflection& reflection = result.reflection;
    uint8_t stage = 0;
    uint32_t parameters = 0;
    if (!r.str(reflection.entry_point) || !r.u8(stage) || stage >= SHADER_STAGE_COUNT)
    {
        return false;
    }
    reflection.stage = static_cast<ShaderStage>(stage);
    if (!read_variables(r, reflection.inputs) || !read_variables(r, reflection.outputs) || !r.count(parameters))
    {
        return false;
    }
    reflection.parameters.resize(parameters);
    for (ShaderBinding& binding : reflection.parameters)
    {
        if (!read_binding(r, binding))
        {
            return false;
        }
    }
    for (uint32_t& extent : reflection.thread_group_size)
    {
        if (!r.u32(extent))
        {
            return false;
        }
    }
    if (!r.at_end())
    {
        return false;
    }
    out = std::move(result);
    return true;
}

bool read_shader_output(const IShaderBinaryStore& store, ShaderHash hash, ShaderCompilerOutput& out)
{
    std::vector<uint8_t> payload;
    if (!store.read(ShaderStoreKind::Binary, hash, payload) || !deserialise_shader_output(payload.data(), payload.size(), out))
    {
        return false;
    }
    out.hash = hash;
    return true;
}

void write_shader_output(const IShaderBinaryStore& store, ShaderHash hash, const ShaderCompilerOutput& output)
{
    const std::vector<uint8_t> payload = serialise_shader_output(output);
    store.write(ShaderStoreKind::Binary, hash, payload.data(), payload.size());
}

} // namespace oryx
