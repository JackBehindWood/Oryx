#include "oxpch.h"
#include "Oryx/Assets/Import/CompiledAssetStore.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Core/Fnv.h"
#include "Oryx/Core/Log.h"

namespace oryx
{

namespace
{

constexpr uint32_t MAGIC = 0x4F584341;
constexpr uint32_t FORMAT_VERSION = 1;
constexpr size_t MAX_TYPE_LENGTH = 19;
constexpr std::string_view TEMP_MARKER = ".tmp";

struct Header
{
    uint32_t magic;
    uint32_t format_version;
    uint32_t importer_version;
    char type[MAX_TYPE_LENGTH + 1];
    uint64_t key_hash;
    uint64_t length;
    uint64_t checksum;
};

static_assert(sizeof(Header) == 56);

std::map<std::string, std::function<uint32_t()>>& registered_types()
{
    // Function-local static: registrars in other translation units run before main in any order.
    static std::map<std::string, std::function<uint32_t()>> types;
    return types;
}

uint64_t checksum_of(const uint8_t* payload, size_t size)
{
    Fnv1a hash;
    hash.mix(payload, size);
    return hash.value();
}

std::string hex(uint64_t value)
{
    char text[17];
    std::snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(value));
    return text;
}

bool is_path_safe_type(std::string_view type)
{
    return !type.empty() && type.size() <= MAX_TYPE_LENGTH && std::all_of(type.begin(), type.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); });
}

std::string extension_of(std::string_view type)
{
    return ".ox" + std::string(type);
}

Header make_header(const CompiledAssetKey& key, const uint8_t* payload, size_t size)
{
    Header header{};
    header.magic = MAGIC;
    header.format_version = FORMAT_VERSION;
    header.importer_version = key.version;
    std::memcpy(header.type, key.type.data(), key.type.size());
    header.key_hash = hash_compiled_asset_key(key);
    header.length = size;
    header.checksum = checksum_of(payload, size);
    return header;
}

bool same_type(const Header& header, std::string_view type)
{
    return std::string_view(header.type, strnlen(header.type, sizeof(header.type))) == type;
}

bool read_header(std::ifstream& file, uint64_t file_size, Header& header)
{
    if (file_size < sizeof(Header) || !file.read(reinterpret_cast<char*>(&header), sizeof(Header)))
    {
        return false;
    }
    return header.magic == MAGIC && header.format_version == FORMAT_VERSION && header.length == file_size - sizeof(Header);
}

} // namespace

CompiledAssetKey make_compiled_asset_key(std::string_view type, uint32_t version, uint64_t settings_hash, const uint8_t* source, size_t source_size)
{
    if (!is_path_safe_type(type))
    {
        throw Error("compiled asset type '" + std::string(type) + "' is not path-safe", "use at most 19 lowercase letters and digits");
    }
    Fnv1a hash;
    hash.mix(source, source_size);
    return CompiledAssetKey{ std::string(type), version, settings_hash, hash.value() };
}

uint64_t hash_compiled_asset_key(const CompiledAssetKey& key)
{
    Fnv1a hash;
    hash.mix_string(key.type);
    hash.mix_value(key.version);
    hash.mix_value(key.settings_hash);
    hash.mix_value(key.source_hash);
    return hash.value();
}

void register_compiled_type(std::string_view type, std::function<uint32_t()> current_version)
{
    registered_types().insert_or_assign(std::string(type), std::move(current_version));
}

std::map<std::string, uint32_t> compiled_type_versions()
{
    std::map<std::string, uint32_t> versions;
    for (const std::pair<const std::string, std::function<uint32_t()>>& entry : registered_types())
    {
        versions.emplace(entry.first, entry.second());
    }
    return versions;
}

CompiledAssetStore::CompiledAssetStore(std::filesystem::path directory, bool enabled)
    : m_directory(std::move(directory))
    , m_enabled(enabled)
{
}

std::filesystem::path CompiledAssetStore::entry_path(const CompiledAssetKey& key) const
{
    if (!is_path_safe_type(key.type))
    {
        throw Error("compiled asset type '" + key.type + "' is not path-safe");
    }
    return m_directory / key.type / (hex(hash_compiled_asset_key(key)) + extension_of(key.type));
}

bool CompiledAssetStore::read(const CompiledAssetKey& key, std::vector<uint8_t>& payload) const
{
    if (!m_enabled)
    {
        return false;
    }
    std::filesystem::path path = entry_path(key);
    std::vector<uint8_t> bytes;
    bool valid = false;
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
        {
            return false;
        }
        uint64_t size = static_cast<uint64_t>(file.tellg());
        file.seekg(0, std::ios::beg);
        Header header{};
        if (read_header(file, size, header) && same_type(header, key.type) && header.importer_version == key.version && header.key_hash == hash_compiled_asset_key(key))
        {
            bytes.resize(static_cast<size_t>(header.length));
            valid = (bytes.empty() || file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) && checksum_of(bytes.data(), bytes.size()) == header.checksum;
        }
    }
    if (!valid)
    {
        std::error_code error;
        std::filesystem::remove(path, error);
        return false;
    }
    payload = std::move(bytes);
    return true;
}

void CompiledAssetStore::write(const CompiledAssetKey& key, const uint8_t* payload, size_t size) const
{
    if (!m_enabled)
    {
        return;
    }
    std::filesystem::path target = entry_path(key);
    std::filesystem::path temp = target;
    temp += std::string(TEMP_MARKER) + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()));

    std::error_code error;
    std::filesystem::create_directories(target.parent_path(), error);
    bool written = false;
    if (!error)
    {
        Header header = make_header(key, payload, size);
        std::ofstream file(temp, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(&header), sizeof(Header));
        file.write(reinterpret_cast<const char*>(payload), static_cast<std::streamsize>(size));
        file.close();
        written = static_cast<bool>(file);
        if (written)
        {
            std::filesystem::rename(temp, target, error);
            written = !error;
        }
        if (!written)
        {
            std::filesystem::remove(temp, error);
        }
    }
    if (!written && !m_warned.exchange(true))
    {
        OX_WARN("compiled asset directory '{}' is not writable; continuing without compiling", m_directory.generic_string());
    }
}

void CompiledAssetStore::prune() const
{
    std::error_code error;
    if (!m_enabled || !std::filesystem::is_directory(m_directory, error))
    {
        return;
    }
    const std::map<std::string, uint32_t> versions = compiled_type_versions();
    std::vector<std::filesystem::path> stale;
    for (const std::filesystem::directory_entry& folder : std::filesystem::directory_iterator(m_directory, error))
    {
        std::error_code folder_error;
        if (!folder.is_directory(folder_error))
        {
            continue;
        }
        const std::string type = folder.path().filename().string();
        const std::map<std::string, uint32_t>::const_iterator known = versions.find(type);
        for (const std::filesystem::directory_entry& file : std::filesystem::directory_iterator(folder.path(), folder_error))
        {
            std::error_code file_error;
            if (!file.is_regular_file(file_error))
            {
                continue;
            }
            const std::string name = file.path().filename().string();
            bool keep = known != versions.end() && file.path().extension() == extension_of(type) && name.find(TEMP_MARKER) == std::string::npos;
            if (keep)
            {
                std::ifstream stream(file.path(), std::ios::binary);
                Header header{};
                keep = stream && read_header(stream, file.file_size(file_error), header) && same_type(header, type) && header.importer_version == known->second;
            }
            if (!keep)
            {
                stale.push_back(file.path());
            }
        }
    }
    for (const std::filesystem::path& path : stale)
    {
        std::filesystem::remove(path, error);
    }
}

} // namespace oryx
