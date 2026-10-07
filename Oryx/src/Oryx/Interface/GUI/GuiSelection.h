#pragma once

#include "Oryx/Interface/Canvas/ImInput.h"

namespace oryx
{

inline constexpr uint32_t k_no_selection = std::numeric_limits<uint32_t>::max();

// The two ends a shift-click range runs between: where the last plain or toggling click landed (`anchor`) and the item last clicked or moved to (`cursor`).
struct Selection
{
    uint32_t anchor = k_no_selection;
    uint32_t cursor = k_no_selection;
};

static_assert(std::is_trivially_copyable_v<Selection> && std::is_standard_layout_v<Selection>);

// The chosen items are bits in a caller-owned array of 64-bit words, one bit per item index; `selection_words(count)` words hold `count` items.
[[nodiscard]] constexpr uint32_t selection_words(uint32_t count) { return (count + 63u) / 64u; }
[[nodiscard]] bool selection_contains(const uint64_t* words, uint32_t count, uint32_t index);
void selection_set(uint64_t* words, uint32_t count, uint32_t index, bool chosen);
void selection_clear(uint64_t* words, uint32_t count);
[[nodiscard]] uint32_t selection_size(const uint64_t* words, uint32_t count);

// Applies a click on `index` the way file managers and spreadsheets do: plain picks only that item, the shortcut key toggles it, shift picks the run from the anchor (added to the rest with the shortcut key too). Returns whether the set changed.
bool select_click(Selection& selection, uint64_t* words, uint32_t count, uint32_t index, const ImKeys& keys);
// Picks every item; the anchor goes to the first and the cursor to the last.
void select_all(Selection& selection, uint64_t* words, uint32_t count);
// Moves the cursor by `delta` items (clamped) as an arrow key does: plain picks the new item, shift extends the run from the anchor. Returns whether the set changed.
bool select_move(Selection& selection, uint64_t* words, uint32_t count, int32_t delta, const ImKeys& keys);

} // namespace oryx
