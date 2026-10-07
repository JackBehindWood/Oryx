#pragma once

namespace oryx
{

// A widget identity: a 64-bit hash of a label or index scoped by the ids above it. Zero means none.
struct Id
{
    uint64_t value = 0;
};

[[nodiscard]] constexpr bool operator==(Id a, Id b) { return a.value == b.value; }
[[nodiscard]] constexpr bool operator!=(Id a, Id b) { return a.value != b.value; }
[[nodiscard]] constexpr bool is_valid(Id id) { return id.value != 0; }

// Never returns the none id. The same label under the same parent always gives the same id.
[[nodiscard]] Id make_id(std::string_view label, Id parent = {});
// For repeated items (rows of a list) where a label would collide or allocate.
[[nodiscard]] Id make_index_id(uint64_t index, Id parent = {});

} // namespace oryx
