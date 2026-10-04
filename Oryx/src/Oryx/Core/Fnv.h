#pragma once

namespace oryx
{

// FNV-1a over little-endian bytes: stable across platforms and runs, unlike std::hash.
class Fnv1a
{
public:
    void mix(const void* data, size_t size)
    {
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < size; ++i)
        {
            m_hash = (m_hash ^ bytes[i]) * PRIME;
        }
    }

    void mix_value(uint64_t value)
    {
        for (uint32_t i = 0; i < 8; ++i)
        {
            const uint8_t byte = static_cast<uint8_t>(value >> (i * 8));
            mix(&byte, 1);
        }
    }

    // The length prefix keeps adjacent fields from aliasing.
    void mix_string(std::string_view text)
    {
        mix_value(text.size());
        mix(text.data(), text.size());
    }

    [[nodiscard]] uint64_t value() const { return m_hash; }

private:
    static constexpr uint64_t OFFSET = 14695981039346656037ull;
    static constexpr uint64_t PRIME = 1099511628211ull;

    uint64_t m_hash = OFFSET;
};

} // namespace oryx
