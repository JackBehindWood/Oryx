#include "oxpch.h"
#include "Oryx/Shaders/ShaderInclude.h"

namespace oryx
{

namespace
{

std::map<std::string, std::string>& includes()
{
    static std::map<std::string, std::string> map;
    return map;
}

void collect(const std::string& text, std::set<std::string>& names)
{
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line))
    {
        const size_t hash = line.find_first_not_of(" \t");
        if (hash == std::string::npos || line[hash] != '#')
        {
            continue;
        }
        const size_t keyword = line.find("include", hash);
        const size_t open = line.find('"', hash);
        if (keyword == std::string::npos || open == std::string::npos)
        {
            continue;
        }
        const size_t close = line.find('"', open + 1);
        if (close == std::string::npos)
        {
            continue;
        }
        const std::string name = line.substr(open + 1, close - open - 1);
        if (!names.insert(name).second)
        {
            continue;
        }
        if (const std::string* included = find_shader_include(name))
        {
            collect(*included, names);
        }
    }
}

} // namespace

void register_shader_include(const std::string& name, std::string text)
{
    includes()[name] = std::move(text);
}

const std::string* find_shader_include(const std::string& name)
{
    const std::map<std::string, std::string>::const_iterator it = includes().find(name);
    return it == includes().end() ? nullptr : &it->second;
}

std::vector<std::string> shader_include_closure(const std::string& text)
{
    std::set<std::string> names;
    collect(text, names);
    return std::vector<std::string>(names.begin(), names.end());
}

} // namespace oryx
