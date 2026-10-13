#include "oxpch.h"
#include "Oryx/Interface/Workbench/WorkbenchState.h"

#include <yaml-cpp/yaml.h>

namespace oryx
{

struct StateWriter::Impl
{
    YAML::Node root;
};

struct StateReader::Impl
{
    YAML::Node root;
};

StateWriter::StateWriter(uint32_t version)
    : m_impl(create_unique<Impl>())
{
    m_impl->root["version"] = version;
}

StateWriter::~StateWriter() = default;

std::string StateWriter::text() const
{
    YAML::Emitter emitter;
    emitter << m_impl->root;
    return emitter.c_str();
}

void StateWriter::put(std::string_view name, const bool& value)
{
    m_impl->root[std::string(name)] = value;
}

void StateWriter::put(std::string_view name, const int32_t& value)
{
    m_impl->root[std::string(name)] = value;
}

void StateWriter::put(std::string_view name, const uint32_t& value)
{
    m_impl->root[std::string(name)] = value;
}

void StateWriter::put(std::string_view name, const float& value)
{
    m_impl->root[std::string(name)] = value;
}

void StateWriter::put(std::string_view name, const std::string& value)
{
    m_impl->root[std::string(name)] = value;
}

StateReader::StateReader(std::string_view yaml, uint32_t version)
    : m_impl(create_unique<Impl>())
{
    try
    {
        m_impl->root = YAML::Load(std::string(yaml));
        if (!m_impl->root.IsMap())
        {
            return;
        }
        const YAML::Node stored = m_impl->root["version"];
        m_valid = stored.IsScalar() && stored.as<uint32_t>() <= version;
    }
    catch (const std::exception& error)
    {
        OX_CORE_WARN("Workbench state ignored: {}", error.what());
    }
}

StateReader::~StateReader() = default;

namespace
{

template<typename T>
void read_scalar(const YAML::Node& root, bool valid, std::string_view name, T& value)
{
    if (!valid)
    {
        return;
    }
    const YAML::Node node = root[std::string(name)];
    if (!node.IsDefined() || !node.IsScalar())
    {
        return;
    }
    T parsed{};
    if (YAML::convert<T>::decode(node, parsed))
    {
        value = std::move(parsed);
    }
}

} // namespace

void StateReader::get(std::string_view name, bool& value)
{
    read_scalar(m_impl->root, m_valid, name, value);
}

void StateReader::get(std::string_view name, int32_t& value)
{
    read_scalar(m_impl->root, m_valid, name, value);
}

void StateReader::get(std::string_view name, uint32_t& value)
{
    read_scalar(m_impl->root, m_valid, name, value);
}

void StateReader::get(std::string_view name, float& value)
{
    read_scalar(m_impl->root, m_valid, name, value);
}

void StateReader::get(std::string_view name, std::string& value)
{
    read_scalar(m_impl->root, m_valid, name, value);
}

} // namespace oryx
