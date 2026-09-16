#pragma once

namespace oryx
{

using ActionId = uint32_t;

constexpr ActionId INVALID_ACTION = static_cast<ActionId>(-1);

constexpr bool is_valid(ActionId action) 
{ 
    return action != INVALID_ACTION; 
}

inline std::string to_string(ActionId action) 
{
    return std::to_string(action); 
}

} // namespace oryx
