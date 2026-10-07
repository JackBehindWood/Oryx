#pragma once

namespace oryx
{

// The value of the none id; make_im_id never returns it.
inline constexpr uint64_t k_none_id = 0;

// A widget identity: a 64-bit hash of a label or index scoped by the ids above it. The untyped form the shared layer works in; UiId and GuiId wrap it.
struct ImId
{
    uint64_t value = k_none_id;
};

[[nodiscard]] constexpr bool operator==(ImId a, ImId b) { return a.value == b.value; }
[[nodiscard]] constexpr bool operator!=(ImId a, ImId b) { return a.value != b.value; }
[[nodiscard]] constexpr bool is_valid(ImId id) { return id.value != k_none_id; }

// 64-bit FNV-1a; saved layouts may key on these ids, so the constants and byte order are fixed (golden test).
inline constexpr uint64_t k_im_fnv_offset = 14695981039346656037ull;
inline constexpr uint64_t k_im_fnv_prime = 1099511628211ull;

// Never returns the none id. The same label under the same parent always gives the same id.
[[nodiscard]] ImId make_im_id(std::string_view label, ImId parent = {});
// For repeated items (rows of a list) where a label would collide or allocate.
[[nodiscard]] ImId make_im_index_id(uint64_t index, ImId parent = {});

// An id that only one context family accepts; Tag is declared by that family. Converting to or from ImId is explicit.
template<typename Tag>
class TypedId
{
public:
    uint64_t value = k_none_id;

    constexpr TypedId() = default;
    constexpr explicit TypedId(ImId id)
        : value(id.value)
    {
    }

    [[nodiscard]] constexpr ImId im() const { return { value }; }
};

template<typename Tag>
[[nodiscard]] constexpr bool operator==(TypedId<Tag> a, TypedId<Tag> b) { return a.value == b.value; }
template<typename Tag>
[[nodiscard]] constexpr bool operator!=(TypedId<Tag> a, TypedId<Tag> b) { return a.value != b.value; }
template<typename Tag>
[[nodiscard]] constexpr bool is_valid(TypedId<Tag> id) { return id.value != k_none_id; }

} // namespace oryx
