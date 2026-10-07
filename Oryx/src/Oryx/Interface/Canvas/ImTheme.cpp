#include "oxpch.h"
#include "Oryx/Interface/Canvas/ImTheme.h"

#include "Oryx/Core/Error.h"

namespace oryx
{

void add_style_variant(ImTheme& theme, std::string_view name, const ImStyle& style)
{
    const Id id = make_id(name);
    for (uint32_t index = 0; index < theme.variant_count; ++index)
    {
        if (theme.variant_ids[index] == id)
        {
            theme.variants[index] = style;
            return;
        }
    }
    if (theme.variant_count == k_max_style_variants)
    {
        throw Error("ImTheme has no free style variant slot", "at most k_max_style_variants variants");
    }
    theme.variant_ids[theme.variant_count] = id;
    theme.variants[theme.variant_count] = style;
    ++theme.variant_count;
}

const ImStyle& style_for(const ImTheme& theme, Id variant)
{
    for (uint32_t index = 0; index < theme.variant_count; ++index)
    {
        if (theme.variant_ids[index] == variant)
        {
            return theme.variants[index];
        }
    }
    return theme.base;
}

const ImStyle& style_for(const ImTheme& theme, std::string_view variant)
{
    return style_for(theme, make_id(variant));
}

} // namespace oryx
