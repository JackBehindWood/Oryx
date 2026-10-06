#include "oxpch.h"
#include "Oryx/Assets/AssetFile.h"

#include "Oryx/Core/Error.h"

namespace oryx
{

std::vector<uint8_t> read_binary_file(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        throw Error("cannot open file '" + path.generic_string() + "'");
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(bytes.data()), size))
    {
        throw Error("cannot read file '" + path.generic_string() + "'");
    }
    return bytes;
}

} // namespace oryx
