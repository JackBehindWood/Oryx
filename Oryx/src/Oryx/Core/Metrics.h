#pragma once

#include "Oryx/Core/Base.h"

namespace oryx
{

struct Metrics
{
    std::map<std::string, double> values;
};

// Keys ending "_max" combine by maximum instead of sum, so a peak survives merging trials.
[[nodiscard]] inline bool is_max_metric(const std::string& key)
{
    constexpr std::string_view k_max_suffix = "_max";
    return key.size() > k_max_suffix.size() && key.compare(key.size() - k_max_suffix.size(), k_max_suffix.size(), k_max_suffix) == 0;
}

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
        if (is_max_metric(key))
        {
            max_metric(metrics, key, value);
        }
        else
        {
            metrics.values[key] += value;
        }
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
