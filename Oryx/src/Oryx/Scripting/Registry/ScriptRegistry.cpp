#include "oxpch.h"
#include "Oryx/Scripting/Registry/ScriptRegistry.h"

#include "Oryx/Scripting/Support/ScriptUtil.h"

namespace oryx
{

namespace
{

template<typename T>
struct ClobberedLayer
{
    typename Registry<T>::Factory factory;
    EntryInfo info;
    bool scripted = false;
    ScriptOrigin origin;
};

template<typename T>
struct ScriptedEntry
{
    ScriptOrigin origin;
    std::vector<ClobberedLayer<T>> clobbered;
};

template<typename T>
FlatHashMap<std::string, ScriptedEntry<T>, 16>& scripted_entries()
{
    static FlatHashMap<std::string, ScriptedEntry<T>, 16> instance;
    return instance;
}

// A direct Registry<T>::unregister_factory bypasses this table, so its entry is dropped on the next lookup.
template<typename T>
ScriptedEntry<T>* find_scripted(const std::string& id)
{
    ScriptedEntry<T>* entry = scripted_entries<T>().find(id);
    if (entry != nullptr && !Registry<T>::has(id))
    {
        scripted_entries<T>().erase(id);
        return nullptr;
    }
    return entry;
}

template<typename T>
void register_scripted(const char* kind, const std::string& id, const ScriptOrigin& origin, typename Registry<T>::Factory factory, EntryInfo info, bool overwrite)
{
    ScriptedEntry<T>* existing = find_scripted<T>(id);
    bool registered = Registry<T>::has(id);
    bool same_origin = existing != nullptr && existing->origin == origin;
    if (registered && !same_origin && !overwrite)
    {
        std::string owner = existing == nullptr ? std::string("C++") : describe(existing->origin);
        throw ScriptError(std::string("the ") + kind + " '" + id + "' is already registered by " + owner + "; pass overwrite=True to replace it");
    }

    ScriptedEntry<T> entry;
    if (existing != nullptr)
    {
        entry = std::move(*existing);
    }
    if (registered && !same_origin)
    {
        entry.clobbered.push_back(ClobberedLayer<T>{ Registry<T>::factory(id), *Registry<T>::info(id), existing != nullptr, entry.origin });
        std::erase_if(entry.clobbered, [&origin](const ClobberedLayer<T>& layer) { return layer.scripted && layer.origin == origin; });
    }
    entry.origin = origin;

    Registry<T>::register_factory(id, std::move(factory), std::move(info));
    scripted_entries<T>().insert_or_assign(id, std::move(entry));
}

template<typename T>
void unregister_language(const std::string& language)
{
    std::vector<std::string> ids;
    scripted_entries<T>().for_each([&ids](const std::string& id, const ScriptedEntry<T>&) { ids.push_back(id); });

    for (const std::string& id : ids)
    {
        ScriptedEntry<T>* entry = find_scripted<T>(id);
        if (entry == nullptr)
        {
            continue;
        }

        std::erase_if(entry->clobbered, [&language](const ClobberedLayer<T>& layer) { return layer.scripted && layer.origin.language == language; });
        if (entry->origin.language != language)
        {
            continue;
        }

        if (entry->clobbered.empty())
        {
            Registry<T>::unregister_factory(id);
            scripted_entries<T>().erase(id);
            continue;
        }

        ClobberedLayer<T> restored = std::move(entry->clobbered.back());
        entry->clobbered.pop_back();
        Registry<T>::register_factory(id, std::move(restored.factory), std::move(restored.info));
        if (restored.scripted)
        {
            entry->origin = std::move(restored.origin);
        }
        else
        {
            scripted_entries<T>().erase(id);
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
    const ScriptedEntry<IGame>* entry = find_scripted<IGame>(id);
    return entry == nullptr ? nullptr : &entry->origin;
}

const ScriptOrigin* scripted_strategy_origin(const std::string& id)
{
    const ScriptedEntry<IStrategy>* entry = find_scripted<IStrategy>(id);
    return entry == nullptr ? nullptr : &entry->origin;
}

} // namespace oryx
