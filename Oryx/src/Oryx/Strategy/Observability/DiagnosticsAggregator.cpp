#include "DiagnosticsAggregator.h"

#include "Oryx/Core/Error.h"

namespace oryx
{

namespace
{

std::string namespace_of(const std::string& key)
{
    size_t slash = key.find('/');
    if (slash == std::string::npos || slash == 0 || slash + 1 == key.size())
    {
        throw Error("diagnostic key '" + key + "' must be 'namespace/name'");
    }
    std::string name_space = key.substr(0, slash);
    if (name_space == "wins" || name_space == "reward")
    {
        throw Error("diagnostic key '" + key + "' uses the built-in '" + name_space + "' namespace");
    }
    return name_space;
}

} // namespace

void DiagnosticsAggregator::on_decision(const IState&, const Decision& decision)
{
    std::set<std::string> seen;
    for (const auto& [key, value] : decision.extra.values)
    {
        seen.insert(namespace_of(key));
        if (is_max_metric(key))
        {
            max_metric(m_metrics, key, value);
        }
        else
        {
            add_metric(m_metrics, key, value);
        }
    }
    for (const std::string& name_space : seen)
    {
        add_metric(m_metrics, name_space + "/decisions", 1.0);
    }
}

} // namespace oryx
