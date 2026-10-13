#pragma once

#include "Oryx/Core/Base.h"

// What a workbench keeps between runs: a plain struct plus one free function describing its fields to a visitor, so reading, writing and change detection share one list.
//   struct ConsoleState { int32_t level = 0; std::string search; };
//   template<typename Visitor> void reflect(Visitor& v, ConsoleState& s) { v.field("level", s.level); v.field("search", s.search); }
// `field` takes bool, int32_t, uint32_t, float and std::string, and ignores extra arguments (ranges, choices) that the settings visitors of a later step use. A key that is missing or of the wrong type keeps the field's default.
namespace oryx
{

// Appends each field to a YAML mapping and returns it as text; `version` is written first.
class StateWriter
{
public:
    explicit StateWriter(uint32_t version);
    ~StateWriter();
    StateWriter(const StateWriter&) = delete;
    StateWriter& operator=(const StateWriter&) = delete;

    template<typename T, typename... Constraints>
    void field(std::string_view name, T& value, const Constraints&...)
    {
        put(name, value);
    }

    [[nodiscard]] std::string text() const;

private:
    void put(std::string_view name, const bool& value);
    void put(std::string_view name, const int32_t& value);
    void put(std::string_view name, const uint32_t& value);
    void put(std::string_view name, const float& value);
    void put(std::string_view name, const std::string& value);

    struct Impl;
    UniquePtr<Impl> m_impl;
};

// Reads the same mapping. `valid` is false when the text is not a mapping or its version is newer than `version` (every field then keeps its default).
class StateReader
{
public:
    StateReader(std::string_view yaml, uint32_t version);
    ~StateReader();
    StateReader(const StateReader&) = delete;
    StateReader& operator=(const StateReader&) = delete;

    template<typename T, typename... Constraints>
    void field(std::string_view name, T& value, const Constraints&...)
    {
        get(name, value);
    }

    [[nodiscard]] bool valid() const { return m_valid; }

private:
    void get(std::string_view name, bool& value);
    void get(std::string_view name, int32_t& value);
    void get(std::string_view name, uint32_t& value);
    void get(std::string_view name, float& value);
    void get(std::string_view name, std::string& value);

    struct Impl;
    UniquePtr<Impl> m_impl;
    bool m_valid = false;
};

// A hash of every field, to tell cheaply whether the state changed; allocates nothing.
class StateHash
{
public:
    template<typename T, typename... Constraints>
    void field(std::string_view name, T& value, const Constraints&...)
    {
        mix(name.data(), name.size());
        if constexpr (std::is_same_v<T, std::string>)
        {
            mix(value.data(), value.size());
        }
        else
        {
            mix(&value, sizeof(T));
        }
    }

    [[nodiscard]] uint64_t value() const { return m_hash; }

private:
    void mix(const void* data, size_t size)
    {
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < size; ++i)
        {
            m_hash = (m_hash ^ bytes[i]) * 1099511628211ull;
        }
    }

    uint64_t m_hash = 14695981039346656037ull;
};

} // namespace oryx
