#include "oxpch.h"
#include "Oryx/Scripting/ScriptRegistry.h"

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

std::string describe(const ScriptOrigin& origin)
{
    std::string result = origin.language + " module '" + origin.module + "'";
    return origin.source_file.empty() ? result : result + " (" + origin.source_file + ")";
}

template<typename T>
void register_scripted(const char* kind, const std::string& id, const ScriptOrigin& origin, typename Registry<T>::Factory factory, EntryInfo info, bool overwrite)
{
    if (Registry<T>::has(id) && !overwrite)
    {
        const ScriptOrigin* existing = origins<T>().find(id);
        if (existing == nullptr)
        {
            throw ScriptError(std::string("the ") + kind + " '" + id + "' is already registered by C++; pass overwrite=True to replace it");
        }
        if (*existing != origin)
        {
            throw ScriptError(std::string("the ") + kind + " '" + id + "' is already registered by " + describe(*existing) + "; pass overwrite=True to replace it");
        }
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
