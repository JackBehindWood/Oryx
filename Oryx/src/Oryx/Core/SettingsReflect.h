#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Core/Error.h"

// A settings section describes its fields once, to any visitor, with one free function next to the struct:
//   template<typename Visitor> void reflect(Visitor& v, DashboardSettings& s) {
//       v.field("history", s.history, Range{ 1, INT32_MAX }, Doc{ "Records the feed keeps" });
//       v.field("annotations", s.annotations, one_of({ "heat", "chosen", "value" }));
//   }
// `field` takes bool, int32_t, uint32_t, float, std::string, std::filesystem::path, std::vector<std::string> and enums (with `choices<E>({ { "name", E::Value }, ... })`). Constraints are plain data:
// Range (integers), Percent (a float 0..1 written as a whole number 0..100), Doc (a one-line description). `removed` names a key that no longer exists, so the reader can say so.
// The visitor behind `v` is an ISettingsSink, so one description serves the YAML reader, the text accessor behind the console and Save, and the settings panel.
namespace oryx
{

class SettingsNode;

struct Range
{
    int64_t min = INT64_MIN;
    int64_t max = INT64_MAX;
};

struct Percent
{
};

struct Doc
{
    std::string_view text;
};

template<typename E>
struct Choice
{
    std::string_view name;
    E value;
};

template<typename E, size_t N>
struct Choices
{
    Choice<E> items[N];
};

template<typename E, size_t N>
[[nodiscard]] constexpr Choices<E, N> choices(const Choice<E> (&items)[N])
{
    Choices<E, N> result{};
    for (size_t i = 0; i < N; ++i)
    {
        result.items[i] = items[i];
    }
    return result;
}

template<size_t N>
struct OneOf
{
    std::string_view names[N];
};

template<size_t N>
[[nodiscard]] constexpr OneOf<N> one_of(const std::string_view (&names)[N])
{
    OneOf<N> result{};
    for (size_t i = 0; i < N; ++i)
    {
        result.names[i] = names[i];
    }
    return result;
}

// A field as a sink sees it. `choices` lists the allowed names of an enum (the field then arrives as an index into it) or of each entry of a string list; empty means unrestricted.
struct FieldInfo
{
    std::string_view key;
    std::string_view doc;
    int64_t min = INT64_MIN;
    int64_t max = INT64_MAX;
    bool percent = false;
    const std::string_view* choices = nullptr;
    uint32_t choice_count = 0;
};

class ISettingsSink
{
public:
    virtual ~ISettingsSink() = default;

    virtual void field(const FieldInfo& info, bool& value) = 0;
    virtual void field(const FieldInfo& info, int32_t& value) = 0;
    virtual void field(const FieldInfo& info, uint32_t& value) = 0;
    virtual void field(const FieldInfo& info, float& value) = 0;
    virtual void field(const FieldInfo& info, std::string& value) = 0;
    virtual void field(const FieldInfo& info, std::filesystem::path& value) = 0;
    virtual void field(const FieldInfo& info, std::vector<std::string>& value) = 0;
    virtual void removed(std::string_view key, std::string_view message) { static_cast<void>(key); static_cast<void>(message); }
};

namespace settings_detail
{

template<typename T>
struct IsChoices : std::false_type
{
};
template<typename E, size_t N>
struct IsChoices<Choices<E, N>> : std::true_type
{
};

template<typename T>
struct IsOneOf : std::false_type
{
};
template<size_t N>
struct IsOneOf<OneOf<N>> : std::true_type
{
};

template<typename First, typename... Rest>
constexpr const First& first_choices(const First& first, const Rest&... rest)
{
    if constexpr (IsChoices<First>::value)
    {
        return first;
    }
    else
    {
        static_assert(sizeof...(Rest) > 0, "an enum field needs choices<E>({ ... })");
        return first_choices(rest...);
    }
}

} // namespace settings_detail

class SettingsVisitor
{
public:
    explicit SettingsVisitor(ISettingsSink& sink)
        : m_sink(sink)
    {
    }

    template<typename T, typename... Constraints>
    void field(std::string_view key, T& value, const Constraints&... constraints)
    {
        FieldInfo info;
        info.key = key;
        (constrain(info, constraints), ...);
        if constexpr (std::is_enum_v<T>)
        {
            const auto& table = settings_detail::first_choices(constraints...);
            constexpr size_t count = std::extent_v<decltype(table.items)>;
            std::string_view names[count];
            int32_t index = 0;
            for (size_t i = 0; i < count; ++i)
            {
                names[i] = table.items[i].name;
                if (table.items[i].value == value)
                {
                    index = static_cast<int32_t>(i);
                }
            }
            info.choices = names;
            info.choice_count = static_cast<uint32_t>(count);
            m_sink.field(info, index);
            value = table.items[static_cast<size_t>(math::clamp(index, 0, static_cast<int32_t>(count) - 1))].value;
        }
        else
        {
            if constexpr (std::is_same_v<T, int32_t>)
            {
                info.min = math::max<int64_t>(info.min, INT32_MIN);
                info.max = math::min<int64_t>(info.max, INT32_MAX);
            }
            else if constexpr (std::is_same_v<T, uint32_t>)
            {
                info.min = math::max<int64_t>(info.min, 0);
                info.max = math::min<int64_t>(info.max, UINT32_MAX);
            }
            m_sink.field(info, value);
        }
    }

    void removed(std::string_view key, std::string_view message) { m_sink.removed(key, message); }

private:
    static void constrain(FieldInfo& info, const Range& range)
    {
        info.min = range.min;
        info.max = range.max;
    }
    static void constrain(FieldInfo& info, const Percent&)
    {
        info.percent = true;
        info.min = 0;
        info.max = 100;
    }
    static void constrain(FieldInfo& info, const Doc& doc) { info.doc = doc.text; }
    template<typename E, size_t N>
    static void constrain(FieldInfo&, const Choices<E, N>&)
    {
    }
    template<size_t N>
    static void constrain(FieldInfo& info, const OneOf<N>& list)
    {
        info.choices = list.names;
        info.choice_count = static_cast<uint32_t>(N);
    }

    ISettingsSink& m_sink;
};

template<typename T>
concept SettingsReflected = requires(SettingsVisitor& visitor, T& value) { reflect(visitor, value); };

// Reads a section from its YAML mapping into the fields; a wrong type, an out-of-range number or a name outside the choices throws SettingsError("<section>.<key> ...").
class SettingsReader : public ISettingsSink
{
public:
    explicit SettingsReader(const SettingsNode& node);

    void field(const FieldInfo& info, bool& value) override;
    void field(const FieldInfo& info, int32_t& value) override;
    void field(const FieldInfo& info, uint32_t& value) override;
    void field(const FieldInfo& info, float& value) override;
    void field(const FieldInfo& info, std::string& value) override;
    void field(const FieldInfo& info, std::filesystem::path& value) override;
    void field(const FieldInfo& info, std::vector<std::string>& value) override;
    void removed(std::string_view key, std::string_view message) override;

private:
    const SettingsNode& m_node;
};

enum class SettingsKind : uint8_t
{
    Bool,
    Integer,
    Number,
    Text,
    // One of a fixed set of names.
    Choice,
    // A comma-separated list of names or strings.
    List
};

// Fields as text, by key. Collect lists every field, Get finds one, Set parses and validates one (a bad value leaves the field and sets `error`).
class SettingsText : public ISettingsSink
{
public:
    enum class Mode : uint8_t
    {
        Collect,
        Get,
        Set
    };

    struct Entry
    {
        std::string key;
        std::string text;
        std::string doc;
        SettingsKind kind = SettingsKind::Text;
        // Save writes a List as a YAML sequence.
        bool list = false;
        // Enum or string-list fields: the allowed names.
        std::vector<std::string> choices;
    };

    SettingsText() = default;
    SettingsText(Mode mode, std::string_view key, std::string_view text = {});

    void field(const FieldInfo& info, bool& value) override;
    void field(const FieldInfo& info, int32_t& value) override;
    void field(const FieldInfo& info, uint32_t& value) override;
    void field(const FieldInfo& info, float& value) override;
    void field(const FieldInfo& info, std::string& value) override;
    void field(const FieldInfo& info, std::filesystem::path& value) override;
    void field(const FieldInfo& info, std::vector<std::string>& value) override;

    [[nodiscard]] const std::vector<Entry>& entries() const { return m_entries; }
    [[nodiscard]] bool found() const { return m_found; }
    // What Get read.
    [[nodiscard]] const std::string& result() const { return m_result; }
    // The message of a rejected Set; empty when it was accepted.
    [[nodiscard]] const std::string& error() const { return m_error; }

private:
    template<typename T>
    void visit(const FieldInfo& info, T& value, SettingsKind kind);

    Mode m_mode = Mode::Collect;
    std::string m_key;
    std::string m_text;
    std::string m_result;
    std::vector<Entry> m_entries;
    std::string m_error;
    bool m_found = false;
};

// "must be between 1 and 5, got 9", or an empty string when `value` is in range.
[[nodiscard]] std::string check_range(const FieldInfo& info, int64_t value);
// "must be one of a, b, got 'x'", or an empty string when `name` is one of the choices.
[[nodiscard]] std::string check_choice(const FieldInfo& info, std::string_view name);

} // namespace oryx
