#pragma once

#include "Oryx/Core/Settings.h"

namespace oryx
{

// The `assets:` section of the settings file.
struct AssetSettings
{
    // Directories searched in order when resolving a relative asset path; a relative root is relative to the settings file. Empty means the resources root.
    std::vector<std::filesystem::path> roots;

    // Effective only while resources.compiled_enabled is also set.
    bool compiled_enabled = true;

    // 0 runs requests on the main thread inside Assets::update(); larger values are not implemented yet.
    uint32_t worker_threads = 0;
};

template<typename Visitor>
void reflect(Visitor& v, AssetSettings& s)
{
    v.field("roots", s.roots, Doc{ "Directories searched in order for relative asset paths; empty means the resources root" });
    v.field("compiled_enabled", s.compiled_enabled, Doc{ "Use compiled assets (also needs resources.compiled_enabled)" });
    v.field("worker_threads", s.worker_threads, Range{ 0, 256 }, Doc{ "Asset worker threads; 0 runs requests on the main thread" });
}

} // namespace oryx
