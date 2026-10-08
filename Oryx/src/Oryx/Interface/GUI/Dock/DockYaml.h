#pragma once

#include "Oryx/Interface/GUI/Dock/DockLayout.h"

namespace YAML
{
class Node;
}

// Versioned YAML for a DockLayout: an explicit tree, panels by registered name. Every function is non-throwing; a failure logs one line, returns false and leaves the output untouched, so the caller falls back to its default layout.
// Version policy: an added key needs no bump (readers ignore unknown keys); any change of meaning bumps k_dock_version and adds a migration. A file newer than this build is refused, never rewritten.
namespace oryx::gui
{

enum class DockYamlStatus : uint8_t
{
    Ok,
    Missing,
    // Unreadable, unparsable or a value of the wrong shape, type or size.
    Corrupt,
    NewerVersion,
    // Parsed, but the layout fails validate.
    Invalid
};

[[nodiscard]] const char* to_string(DockYamlStatus status);

// Unregistered ids are kept and written as `{hash: N}` so a view registered later still finds its place.
[[nodiscard]] bool serialize_layout_to_string(const DockLayout& layout, const PanelTable& panels, std::string& out);
[[nodiscard]] bool deserialize_layout_from_string(DockLayout& out, const PanelTable& panels, std::string_view yaml, DockYamlStatus* status = nullptr);

// Writes a sibling temp file and renames it over `path`, so a crash never leaves a truncated layout.
[[nodiscard]] bool save_layout_yaml(const DockLayout& layout, const PanelTable& panels, const std::filesystem::path& path);
[[nodiscard]] bool load_layout_yaml(DockLayout& out, const PanelTable& panels, const std::filesystem::path& path, DockYamlStatus* status = nullptr);

// Moves an unreadable layout aside to `<path>.bad` so the next save cannot destroy it.
[[nodiscard]] bool quarantine_layout_file(const std::filesystem::path& path);

inline constexpr uintmax_t k_max_dock_yaml_bytes = 256 * 1024;

namespace detail
{

// Brings a document from version `from` to from + 1.
struct DockMigration
{
    uint32_t from = 0;
    void (*apply)(YAML::Node& root) = nullptr;
};

[[nodiscard]] bool deserialize_with_migrations(DockLayout& out, const PanelTable& panels, std::string_view yaml, const DockMigration* migrations, uint32_t migration_count, DockYamlStatus* status);

}

}
