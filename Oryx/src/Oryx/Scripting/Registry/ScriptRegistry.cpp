#include "oxpch.h"
#include "Oryx/Scripting/Registry/ScriptRegistry.h"

#include "Oryx/Scripting/Support/ScriptUtil.h"

namespace oryx
{

namespace
{

template<typename T>
FlatHashMap<std::string, ScriptOrigin, 16>& origins()
{
    static FlatHashMap<std::string, ScriptOrigin, 16> instance;
    return instance;
}

// A clobbered entry's factory/info, plus its origin (had_origin false for a C++ entry) so
// restoring it also restores whether it counts as scripted and by whom.
template<typename T>
struct StashedEntry
{
    typename Registry<T>::Factory factory;
    EntryInfo info;
    bool had_origin = false;
    ScriptOrigin origin;
};

// What overwrite=True clobbered, so unregister_language can put it back. Only ever holds the
// immediately-clobbered entry per id: a chain of overwrites (A overwrites C++, B overwrites A)
// restores A's factory when B's runtime stops, not the original C++ one - one stash slot per id,
// documented as an acceptable limit rather than a bug.
template<typename T>
FlatHashMap<std::string, StashedEntry<T>, 16>& overwritten()
{
    static FlatHashMap<std::string, StashedEntry<T>, 16> instance;
    return instance;
}

template<typename T>
void register_scripted(const char* kind, const std::string& id, const ScriptOrigin& origin, typename Registry<T>::Factory factory, EntryInfo info, bool overwrite)
{
    const ScriptOrigin* existing_origin = origins<T>().find(id);
    if (Registry<T>::has(id) && !overwrite)
    {
        if (existing_origin == nullptr)
        {
            throw ScriptError(std::string("the ") + kind + " '" + id + "' is already registered by C++; pass overwrite=True to replace it");
        }
        if (*existing_origin != origin)
        {
            throw ScriptError(std::string("the ") + kind + " '" + id + "' is already registered by " + describe(*existing_origin) + "; pass overwrite=True to replace it");
        }
    }

    // Always overwrites any earlier stash on a fresh cross-origin clobber, so a chain of
    // overwrites keeps only the immediately-clobbered entry, not the first one ever displaced.
    bool clobbers_other_origin = Registry<T>::has(id) && (existing_origin == nullptr || *existing_origin != origin);
    if (clobbers_other_origin)
    {
        StashedEntry<T> stashed{ Registry<T>::factory(id), *Registry<T>::info(id), existing_origin != nullptr, existing_origin != nullptr ? *existing_origin : ScriptOrigin{} };
        overwritten<T>().insert_or_assign(id, std::move(stashed));
    }

    Registry<T>::register_factory(id, std::move(factory), std::move(info));
    origins<T>().insert_or_assign(id, origin);
}

template<typename T>
void unregister_language(const std::string& language)
{
    std::vector<std::string> ids;
    origins<T>().for_each([&](const std::string& id, const ScriptOrigin& origin)
    {
        if (origin.language == language)
        {
            ids.push_back(id);
        }
    });

    for (const std::string& id : ids)
    {
        Registry<T>::unregister_factory(id);
        origins<T>().erase(id);

        if (const StashedEntry<T>* stashed = overwritten<T>().find(id))
        {
            Registry<T>::register_factory(id, stashed->factory, stashed->info);
            if (stashed->had_origin)
            {
                origins<T>().insert_or_assign(id, stashed->origin);
            }
            overwritten<T>().erase(id);
        }
    }
}

} // namespace

void register_scripted_game(const std::string& id, const ScriptOrigin& origin, GameRegistry::Factory factory, EntryInfo info, bool overwrite)
{
    register_scripted<IGame>("game", id, origin, std::move(factory), std::move(info), overwrite);
}

void register_scripted_strategy(const std::string& id, const ScriptOrigin& origin, StrategyRegistry::Factory factory, EntryInfo info, bool overwrite)
{
    register_scripted<IStrategy>("strategy", id, origin, std::move(factory), std::move(info), overwrite);
}

void unregister_scripted(const std::string& language)
{
    unregister_language<IGame>(language);
    unregister_language<IStrategy>(language);
}

const ScriptOrigin* scripted_game_origin(const std::string& id)
{
    return origins<IGame>().find(id);
}

const ScriptOrigin* scripted_strategy_origin(const std::string& id)
{
    return origins<IStrategy>().find(id);
}

} // namespace oryx
