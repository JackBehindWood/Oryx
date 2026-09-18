#pragma once

namespace oryx
{

// Data-only (§5) key/value slot type, used by FlatHashMap.
template<typename Key, typename Value>
struct Pair
{
    Key key;
    Value value;
};

} // namespace oryx
