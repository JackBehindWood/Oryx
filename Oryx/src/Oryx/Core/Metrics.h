#pragma once

#include "Oryx/Core/Base.h"

namespace oryx
{

struct Metrics
{
    std::map<std::string, double> values;
};

inline void add_metric(Metrics& metrics, const std::string& key, double value)
{
    metrics.values[key] += value;
}

inline void max_metric(Metrics& metrics, const std::string& key, double value)
{
    std::map<std::string, double>::iterator found = metrics.values.find(key);
    if (found == metrics.values.end())
    {
        metrics.values.emplace(key, value);
    }
    else if (value > found->second)
    {
        found->second = value;
    }
}

inline void merge(Metrics& metrics, const Metrics& other)
{
    for (const auto& [key, value] : other.values)
    {
        metrics.values[key] += value;
    }
}

[[nodiscard]] inline bool has_metric(const Metrics& metrics, const std::string& key)
{
    return metrics.values.find(key) != metrics.values.end();
}

// 0 for an absent key, so a metric a trial never produced reads as nothing counted.
[[nodiscard]] inline double get_metric(const Metrics& metrics, const std::string& key)
{
    std::map<std::string, double>::const_iterator found = metrics.values.find(key);
    return found == metrics.values.end() ? 0.0 : found->second;
}

} // namespace oryx
