#include "oxpch.h"
#include "Oryx/Interface/GUI/Dock/DockYaml.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Core/Log.h"
#include "Oryx/Interface/GUI/Dock/DockOps.h"

#include <yaml-cpp/yaml.h>

namespace oryx::gui
{

namespace
{

struct Reject
{
    DockYamlStatus status;
    std::string why;
};

[[noreturn]] void reject(DockYamlStatus status, std::string why) { throw Reject{ status, std::move(why) }; }
[[noreturn]] void corrupt(std::string why) { reject(DockYamlStatus::Corrupt, std::move(why)); }

constexpr uint32_t k_max_yaml_depth = k_max_dock_nodes;

const char* axis_name(DockAxis axis) { return axis == DockAxis::Horizontal ? "horizontal" : "vertical"; }

const char* mode_name(DockSizeMode mode)
{
    switch (mode)
    {
    case DockSizeMode::Ratio: return "ratio";
    case DockSizeMode::FixedFirst: return "fixed_first";
    case DockSizeMode::FixedSecond: return "fixed_second";
    }
    return "ratio";
}

const char* zone_name(DropZone zone)
{
    switch (zone)
    {
    case DropZone::Centre: return "centre";
    case DropZone::Left: return "left";
    case DropZone::Right: return "right";
    case DropZone::Top: return "top";
    case DropZone::Bottom: return "bottom";
    }
    return "centre";
}

// Shortest decimal that parses back to the same float, so files stay readable and still round-trip bit-exactly.
std::string format_float(float value)
{
    char buffer[32];
    for (int32_t precision = 1; precision <= 9; ++precision)
    {
        std::snprintf(buffer, sizeof(buffer), "%.*g", precision, static_cast<double>(value));
        if (std::strtof(buffer, nullptr) == value)
            break;
    }
    return buffer;
}

YAML::Node panel_node(const PanelTable& panels, PanelId id)
{
    const PanelDesc* desc = find_panel(panels, id);
    if (desc != nullptr)
        return YAML::Node(std::string(desc->name));
    YAML::Node node(YAML::NodeType::Map);
    node["hash"] = id.hash;
    return node;
}

YAML::Node write_tree(const DockLayout& layout, const PanelTable& panels, int32_t index, uint32_t depth)
{
    if (index < 0 || static_cast<uint32_t>(index) >= layout.node_count || depth > k_max_yaml_depth)
        corrupt("layout tree is out of range");
    const DockNode& node = layout.nodes[index];
    YAML::Node wrapper(YAML::NodeType::Map);
    YAML::Node body(YAML::NodeType::Map);
    if (node.kind == DockNodeKind::Tabs)
    {
        YAML::Node list(YAML::NodeType::Sequence);
        for (uint32_t t = 0; t < node.count && t < k_max_dock_tabs; ++t)
            list.push_back(panel_node(panels, node.tabs[t]));
        list.SetStyle(YAML::EmitterStyle::Flow);
        body["panels"] = list;
        body["selected"] = static_cast<uint32_t>(node.selected);
        body["collapsed"] = node.collapsed != 0;
        wrapper["tabs"] = body;
        return wrapper;
    }
    if (node.kind != DockNodeKind::Split)
        corrupt("layout contains an empty node");
    body["axis"] = axis_name(node.axis);
    body["mode"] = mode_name(node.mode);
    body["ratio"] = format_float(node.ratio);
    body["points"] = format_float(node.points);
    body["first"] = write_tree(layout, panels, node.first, depth + 1);
    body["second"] = write_tree(layout, panels, node.second, depth + 1);
    wrapper["split"] = body;
    return wrapper;
}

YAML::Node write_document(const DockLayout& layout, const PanelTable& panels)
{
    YAML::Node doc(YAML::NodeType::Map);
    doc["version"] = k_dock_version;

    YAML::Node surfaces(YAML::NodeType::Sequence);
    for (uint32_t s = 0; s < k_max_dock_surfaces; ++s)
    {
        if (layout.roots[s] == k_no_node)
            continue;
        YAML::Node entry(YAML::NodeType::Map);
        entry["surface"] = s;
        entry["root"] = write_tree(layout, panels, layout.roots[s], 0);
        surfaces.push_back(entry);
    }
    doc["surfaces"] = surfaces;

    YAML::Node floats(YAML::NodeType::Sequence);
    for (uint32_t f = 0; f < layout.float_count && f < k_max_dock_floats; ++f)
    {
        const DockFloat& item = layout.floats[f];
        YAML::Node entry(YAML::NodeType::Map);
        entry["panel"] = panel_node(panels, item.panel);
        entry["surface"] = static_cast<uint32_t>(item.surface);
        YAML::Node rect(YAML::NodeType::Sequence);
        rect.push_back(format_float(item.rect.min[0]));
        rect.push_back(format_float(item.rect.min[1]));
        rect.push_back(format_float(item.rect.size[0]));
        rect.push_back(format_float(item.rect.size[1]));
        rect.SetStyle(YAML::EmitterStyle::Flow);
        entry["rect"] = rect;
        floats.push_back(entry);
    }
    doc["floats"] = floats;

    YAML::Node homes(YAML::NodeType::Sequence);
    for (uint32_t h = 0; h < layout.home_count && h < k_max_dock_homes; ++h)
    {
        const DockHome& item = layout.homes[h];
        YAML::Node entry(YAML::NodeType::Map);
        entry["panel"] = panel_node(panels, item.panel);
        entry["sibling"] = panel_node(panels, item.sibling);
        entry["zone"] = zone_name(item.zone);
        homes.push_back(entry);
    }
    doc["homes"] = homes;

    YAML::Node closed(YAML::NodeType::Sequence);
    for (uint32_t c = 0; c < layout.closed_count && c < k_max_dock_closed; ++c)
        closed.push_back(panel_node(panels, layout.closed[c]));
    doc["closed"] = closed;
    return doc;
}

YAML::Node get(const YAML::Node& node, const char* key)
{
    if (!node.IsMap())
        corrupt(std::string("expected a mapping around '") + key + "'");
    const YAML::Node value = node[key];
    if (!value.IsDefined())
        corrupt(std::string("missing key '") + key + "'");
    return value;
}

YAML::Node get_optional(const YAML::Node& node, const char* key)
{
    if (!node.IsMap())
        corrupt(std::string("expected a mapping around '") + key + "'");
    return node[key];
}

std::string as_text(const YAML::Node& node, const char* what)
{
    if (!node.IsScalar())
        corrupt(std::string(what) + " must be a scalar");
    return node.Scalar();
}

uint32_t as_uint(const YAML::Node& node, const char* what, uint32_t max)
{
    if (!node.IsScalar())
        corrupt(std::string(what) + " must be a number");
    int64_t value = 0;
    if (!YAML::convert<int64_t>::decode(node, value) || value < 0 || value > static_cast<int64_t>(max))
        corrupt(std::string(what) + " must be an integer in [0, " + std::to_string(max) + "]");
    return static_cast<uint32_t>(value);
}

float as_float(const YAML::Node& node, const char* what)
{
    double value = 0.0;
    if (!node.IsScalar() || !YAML::convert<double>::decode(node, value) || !std::isfinite(value) || std::fabs(value) > static_cast<double>(std::numeric_limits<float>::max()))
        corrupt(std::string(what) + " must be a finite number");
    return static_cast<float>(value);
}

bool as_bool(const YAML::Node& node, const char* what)
{
    bool value = false;
    if (!node.IsScalar() || !YAML::convert<bool>::decode(node, value))
        corrupt(std::string(what) + " must be true or false");
    return value;
}

PanelId read_panel(const YAML::Node& node, bool allow_none = false)
{
    if (node.IsMap())
    {
        const uint32_t hash = as_uint(get(node, "hash"), "panel hash", std::numeric_limits<uint32_t>::max());
        if (hash == 0 && allow_none)
            return PanelId{};
        if (hash == 0)
            corrupt("panel hash 0 is reserved");
        return PanelId{ hash };
    }
    const std::string name = as_text(node, "panel");
    if (name.empty())
        corrupt("panel name is empty");
    return make_panel_id(name);
}

DockAxis read_axis(const std::string& text)
{
    if (text == "horizontal")
        return DockAxis::Horizontal;
    if (text == "vertical")
        return DockAxis::Vertical;
    corrupt("unknown axis '" + text + "'");
}

DockSizeMode read_mode(const std::string& text)
{
    if (text == "ratio")
        return DockSizeMode::Ratio;
    if (text == "fixed_first")
        return DockSizeMode::FixedFirst;
    if (text == "fixed_second")
        return DockSizeMode::FixedSecond;
    corrupt("unknown size mode '" + text + "'");
}

DropZone read_zone(const std::string& text)
{
    if (text == "centre")
        return DropZone::Centre;
    if (text == "left")
        return DropZone::Left;
    if (text == "right")
        return DropZone::Right;
    if (text == "top")
        return DropZone::Top;
    if (text == "bottom")
        return DropZone::Bottom;
    corrupt("unknown drop zone '" + text + "'");
}

// Appends depth-first; the node budget is the pool, so an alias bomb or a deep chain stops at k_max_dock_nodes visits.
int32_t read_tree(DockLayout& layout, const YAML::Node& node, uint32_t depth)
{
    if (depth > k_max_yaml_depth)
        corrupt("layout tree is too deep");
    if (layout.node_count >= k_max_dock_nodes)
        corrupt("layout has more than " + std::to_string(k_max_dock_nodes) + " nodes");
    if (!node.IsMap() || node.size() != 1)
        corrupt("a layout node must be a mapping with one key, 'split' or 'tabs'");

    const int32_t index = static_cast<int32_t>(layout.node_count++);
    if (node["tabs"].IsDefined())
    {
        const YAML::Node body = node["tabs"];
        const YAML::Node list = get(body, "panels");
        if (!list.IsSequence() || list.size() < 1 || list.size() > k_max_dock_tabs)
            corrupt("tabs 'panels' must hold 1 to " + std::to_string(k_max_dock_tabs) + " entries");
        DockNode tabs;
        tabs.kind = DockNodeKind::Tabs;
        for (std::size_t i = 0; i < list.size(); ++i)
            tabs.tabs[tabs.count++] = read_panel(list[i]);
        tabs.selected = static_cast<uint8_t>(as_uint(get(body, "selected"), "tabs selected", tabs.count - 1u));
        const YAML::Node collapsed = get_optional(body, "collapsed");
        tabs.collapsed = collapsed.IsDefined() && as_bool(collapsed, "tabs collapsed") ? 1 : 0;
        layout.nodes[index] = tabs;
        return index;
    }
    if (!node["split"].IsDefined())
        corrupt("a layout node must be 'split' or 'tabs'");

    const YAML::Node body = node["split"];
    DockNode split;
    split.kind = DockNodeKind::Split;
    split.axis = read_axis(as_text(get(body, "axis"), "split axis"));
    split.mode = read_mode(as_text(get(body, "mode"), "split mode"));
    split.ratio = as_float(get(body, "ratio"), "split ratio");
    split.points = as_float(get(body, "points"), "split points");
    layout.nodes[index] = split;
    const int32_t first = read_tree(layout, get(body, "first"), depth + 1);
    const int32_t second = read_tree(layout, get(body, "second"), depth + 1);
    layout.nodes[index].first = first;
    layout.nodes[index].second = second;
    return index;
}

void read_document(DockLayout& layout, const YAML::Node& doc)
{
    const YAML::Node surfaces = get(doc, "surfaces");
    if (!surfaces.IsSequence() || surfaces.size() > k_max_dock_surfaces)
        corrupt("'surfaces' must be a list of at most " + std::to_string(k_max_dock_surfaces));
    for (std::size_t i = 0; i < surfaces.size(); ++i)
    {
        const uint32_t surface = as_uint(get(surfaces[i], "surface"), "surface", k_max_dock_surfaces - 1);
        if (layout.roots[surface] != k_no_node)
            corrupt("surface " + std::to_string(surface) + " appears twice");
        layout.roots[surface] = read_tree(layout, get(surfaces[i], "root"), 0);
    }

    const YAML::Node floats = get_optional(doc, "floats");
    if (floats.IsDefined())
    {
        if (!floats.IsSequence() || floats.size() > k_max_dock_floats)
            corrupt("'floats' must be a list of at most " + std::to_string(k_max_dock_floats));
        for (std::size_t i = 0; i < floats.size(); ++i)
        {
            DockFloat& item = layout.floats[layout.float_count++];
            item.panel = read_panel(get(floats[i], "panel"));
            const YAML::Node surface = get_optional(floats[i], "surface");
            item.surface = surface.IsDefined() ? static_cast<uint8_t>(as_uint(surface, "float surface", k_max_dock_surfaces - 1)) : 0;
            const YAML::Node rect = get(floats[i], "rect");
            if (!rect.IsSequence() || rect.size() != 4)
                corrupt("float 'rect' must be [x, y, width, height]");
            item.rect.min = Vec2f(as_float(rect[0], "rect x"), as_float(rect[1], "rect y"));
            item.rect.size = Vec2f(as_float(rect[2], "rect width"), as_float(rect[3], "rect height"));
        }
    }

    const YAML::Node homes = get_optional(doc, "homes");
    if (homes.IsDefined())
    {
        if (!homes.IsSequence() || homes.size() > k_max_dock_homes)
            corrupt("'homes' must be a list of at most " + std::to_string(k_max_dock_homes));
        for (std::size_t i = 0; i < homes.size(); ++i)
        {
            DockHome& item = layout.homes[layout.home_count++];
            item.panel = read_panel(get(homes[i], "panel"));
            item.sibling = read_panel(get(homes[i], "sibling"), true);
            item.zone = read_zone(as_text(get(homes[i], "zone"), "home zone"));
        }
    }

    const YAML::Node closed = get_optional(doc, "closed");
    if (closed.IsDefined())
    {
        if (!closed.IsSequence() || closed.size() > k_max_dock_closed)
            corrupt("'closed' must be a list of at most " + std::to_string(k_max_dock_closed));
        for (std::size_t i = 0; i < closed.size(); ++i)
            layout.closed[layout.closed_count++] = read_panel(closed[i]);
    }
}

void migrate(YAML::Node& doc, uint32_t version, const detail::DockMigration* migrations, uint32_t count)
{
    while (version < k_dock_version)
    {
        const detail::DockMigration* step = nullptr;
        for (uint32_t i = 0; i < count; ++i)
            if (migrations[i].from == version && migrations[i].apply != nullptr)
                step = &migrations[i];
        if (step == nullptr)
            corrupt("no migration from layout version " + std::to_string(version));
        step->apply(doc);
        ++version;
        doc["version"] = version;
    }
}

void report(DockYamlStatus* status, DockYamlStatus value)
{
    if (status != nullptr)
        *status = value;
}

}

const char* to_string(DockYamlStatus status)
{
    switch (status)
    {
    case DockYamlStatus::Ok: return "ok";
    case DockYamlStatus::Missing: return "missing";
    case DockYamlStatus::Corrupt: return "corrupt";
    case DockYamlStatus::NewerVersion: return "newer version";
    case DockYamlStatus::Invalid: return "invalid";
    }
    return "unknown";
}

bool serialize_layout_to_string(const DockLayout& layout, const PanelTable& panels, std::string& out)
{
    try
    {
        YAML::Emitter emitter;
        emitter << write_document(layout, panels);
        if (!emitter.good())
            corrupt(emitter.GetLastError());
        out = emitter.c_str();
        out += '\n';
        return true;
    }
    catch (const Reject& error)
    {
        OX_ERROR("Dock layout not written: {}", error.why);
    }
    catch (const std::exception& error)
    {
        OX_ERROR("Dock layout not written: {}", error.what());
    }
    return false;
}

namespace detail
{

bool deserialize_with_migrations(DockLayout& out, const PanelTable& panels, std::string_view yaml, const DockMigration* migrations, uint32_t migration_count, DockYamlStatus* status)
{
    report(status, DockYamlStatus::Corrupt);
    try
    {
        if (yaml.size() > k_max_dock_yaml_bytes)
            corrupt("document is larger than " + std::to_string(k_max_dock_yaml_bytes) + " bytes");
        YAML::Node doc = YAML::Load(std::string(yaml));
        const uint32_t version = as_uint(get(doc, "version"), "version", std::numeric_limits<uint32_t>::max());
        if (version > k_dock_version)
            reject(DockYamlStatus::NewerVersion, "layout version " + std::to_string(version) + " is newer than this build (" + std::to_string(k_dock_version) + ")");
        migrate(doc, version, migrations, migration_count);

        auto scratch = std::make_unique<DockLayout>();
        read_document(*scratch, doc);
        scratch->version = k_dock_version;
        normalize(*scratch, panels);
        try
        {
            validate(*scratch, panels, ValidateFlags{});
        }
        catch (const oryx::Error& error)
        {
            reject(DockYamlStatus::Invalid, error.what());
        }
        out = *scratch;
        report(status, DockYamlStatus::Ok);
        return true;
    }
    catch (const Reject& error)
    {
        report(status, error.status);
        OX_ERROR("Dock layout rejected ({}): {}", to_string(error.status), error.why);
    }
    catch (const std::exception& error)
    {
        OX_ERROR("Dock layout rejected (corrupt): {}", error.what());
    }
    return false;
}

}

namespace
{

constexpr detail::DockMigration k_migrations[] = { { 0, nullptr } };

}

bool deserialize_layout_from_string(DockLayout& out, const PanelTable& panels, std::string_view yaml, DockYamlStatus* status)
{
    return detail::deserialize_with_migrations(out, panels, yaml, k_migrations, 0, status);
}

bool save_layout_yaml(const DockLayout& layout, const PanelTable& panels, const std::filesystem::path& path)
{
    std::string text;
    if (!serialize_layout_to_string(layout, panels, text))
        return false;

    std::error_code ec;
    if (path.has_parent_path())
        std::filesystem::create_directories(path.parent_path(), ec);
    std::filesystem::path temp = path;
    temp += ".tmp";
    {
        std::ofstream file(temp, std::ios::binary | std::ios::trunc);
        file << text;
        file.flush();
        if (!file.good())
        {
            file.close();
            std::filesystem::remove(temp, ec);
            OX_ERROR("Dock layout '{}' not saved: write failed", path.string());
            return false;
        }
    }
    std::filesystem::rename(temp, path, ec);
    if (ec)
    {
        std::filesystem::remove(temp, ec);
        OX_ERROR("Dock layout '{}' not saved: rename failed", path.string());
        return false;
    }
    return true;
}

bool load_layout_yaml(DockLayout& out, const PanelTable& panels, const std::filesystem::path& path, DockYamlStatus* status)
{
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec))
    {
        report(status, DockYamlStatus::Missing);
        return false;
    }
    const uintmax_t size = std::filesystem::file_size(path, ec);
    if (ec || size > k_max_dock_yaml_bytes)
    {
        report(status, DockYamlStatus::Corrupt);
        OX_ERROR("Dock layout '{}' rejected (corrupt): unreadable or larger than {} bytes", path.string(), k_max_dock_yaml_bytes);
        return false;
    }
    std::ifstream file(path, std::ios::binary);
    std::string text(static_cast<std::size_t>(size), '\0');
    file.read(text.data(), static_cast<std::streamsize>(size));
    if (!file.good() && !file.eof())
    {
        report(status, DockYamlStatus::Corrupt);
        OX_ERROR("Dock layout '{}' rejected (corrupt): read failed", path.string());
        return false;
    }
    return deserialize_layout_from_string(out, panels, text, status);
}

bool quarantine_layout_file(const std::filesystem::path& path)
{
    std::error_code ec;
    std::filesystem::path bad = path;
    bad += ".bad";
    std::filesystem::rename(path, bad, ec);
    return !ec;
}

}
