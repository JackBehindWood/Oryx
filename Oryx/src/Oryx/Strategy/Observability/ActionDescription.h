#pragma once

#include "Oryx/Game/ActionId.h"
#include "Oryx/Game/IState.h"

namespace oryx
{

constexpr size_t k_label_capacity = 24;
constexpr size_t k_record_spaces = 8;

struct FixedLabel
{
    char text[k_label_capacity] = {};
    uint8_t length = 0;
};

inline void set_label(FixedLabel& label, std::string_view text)
{
    size_t length = text.size() < k_label_capacity ? text.size() : k_label_capacity;
    // Never cut inside a multi-byte UTF-8 sequence.
    while (length < text.size() && length > 0 && (static_cast<uint8_t>(text[length]) & 0xC0) == 0x80)
    {
        --length;
    }
    std::memcpy(label.text, text.data(), length);
    label.length = static_cast<uint8_t>(length);
}

[[nodiscard]] inline std::string_view label_view(const FixedLabel& label)
{
    return std::string_view(label.text, label.length);
}

// What a person calls an action and the board spaces it touches, in the order they are picked; spaces are plain indices so Dashboard names no board type.
struct ActionDescription
{
    FixedLabel label;
    uint32_t spaces[k_record_spaces] = {};
    uint8_t space_count = 0;
};

// Supplied by whoever knows the game's vocabulary (the board presenter); the feed calls it at decision time because the state is gone afterwards.
class IActionDescriber
{
public:
    virtual ~IActionDescriber() = default;

    // False leaves `out` unspecified and makes the feed fall back to IState::action_to_string.
    virtual bool describe(const IState& state, ActionId action, ActionDescription& out) const = 0;
};

} // namespace oryx
