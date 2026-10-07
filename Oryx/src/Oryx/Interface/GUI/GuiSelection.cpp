#include "oxpch.h"
#include "Oryx/Interface/GUI/GuiSelection.h"

namespace oryx
{

namespace
{

void set_range(uint64_t* words, uint32_t count, uint32_t from, uint32_t to)
{
    for (uint32_t index = math::min(from, to); index <= math::max(from, to) && index < count; ++index)
    {
        selection_set(words, count, index, true);
    }
}

} // namespace

bool selection_contains(const uint64_t* words, uint32_t count, uint32_t index)
{
    return index < count && ((words[index / 64u] >> (index % 64u)) & 1u) != 0;
}

void selection_set(uint64_t* words, uint32_t count, uint32_t index, bool chosen)
{
    if (index >= count)
    {
        return;
    }
    const uint64_t bit = uint64_t{ 1 } << (index % 64u);
    words[index / 64u] = chosen ? (words[index / 64u] | bit) : (words[index / 64u] & ~bit);
}

void selection_clear(uint64_t* words, uint32_t count)
{
    std::fill(words, words + selection_words(count), uint64_t{ 0 });
}

uint32_t selection_size(const uint64_t* words, uint32_t count)
{
    uint32_t total = 0;
    for (uint32_t index = 0; index < count; ++index)
    {
        total += selection_contains(words, count, index) ? 1u : 0u;
    }
    return total;
}

bool select_click(Selection& selection, uint64_t* words, uint32_t count, uint32_t index, const ImKeys& keys)
{
    if (index >= count)
    {
        return false;
    }
    const uint32_t before = selection_size(words, count);
    const bool was_chosen = selection_contains(words, count, index);
    if (keys.shift && selection.anchor != k_no_selection && selection.anchor < count)
    {
        if (!keys.shortcut)
        {
            selection_clear(words, count);
        }
        set_range(words, count, selection.anchor, index);
        selection.cursor = index;
    }
    else if (keys.shortcut)
    {
        selection_set(words, count, index, !was_chosen);
        selection.anchor = index;
        selection.cursor = index;
    }
    else
    {
        selection_clear(words, count);
        selection_set(words, count, index, true);
        selection.anchor = index;
        selection.cursor = index;
        return before != 1 || !was_chosen;
    }
    return before != selection_size(words, count) || was_chosen != selection_contains(words, count, index);
}

void select_all(Selection& selection, uint64_t* words, uint32_t count)
{
    selection_clear(words, count);
    if (count == 0)
    {
        selection = {};
        return;
    }
    set_range(words, count, 0, count - 1);
    selection.anchor = 0;
    selection.cursor = count - 1;
}

bool select_move(Selection& selection, uint64_t* words, uint32_t count, int32_t delta, const ImKeys& keys)
{
    if (count == 0)
    {
        return false;
    }
    const int64_t from = selection.cursor == k_no_selection ? (delta >= 0 ? -1 : static_cast<int64_t>(count)) : static_cast<int64_t>(selection.cursor);
    const uint32_t to = static_cast<uint32_t>(math::clamp<int64_t>(from + delta, 0, static_cast<int64_t>(count) - 1));
    ImKeys plain = keys;
    plain.shortcut = false;
    if (plain.shift && selection.anchor == k_no_selection)
    {
        selection.anchor = to;
    }
    return select_click(selection, words, count, to, plain);
}

} // namespace oryx
