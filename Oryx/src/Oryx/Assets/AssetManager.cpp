#include "oxpch.h"
#include "Oryx/Assets/AssetManager.h"

#include "Oryx/Assets/Execution/DeferredExecutor.h"

namespace oryx
{

AssetManager::AssetManager(const AssetSettings& settings, UniquePtr<IAssetExecutor> executor)
    : m_settings(settings)
    , m_cache(settings.cache_dir, settings.cache_enabled)
    , m_executor(executor != nullptr ? std::move(executor) : UniquePtr<IAssetExecutor>(create_unique<DeferredExecutor>()))
{
}

AssetManager::~AssetManager() = default;

size_t AssetManager::live_assets() const
{
    size_t total = 0;
    for (const std::pair<const std::type_index, UniquePtr<detail::AssetTableBase>>& entry : m_tables)
    {
        total += entry.second->live_count();
    }
    return total;
}

std::filesystem::path AssetManager::resolve_path(const std::filesystem::path& path) const
{
    if (path.is_absolute())
    {
        return path;
    }
    for (const std::filesystem::path& root : m_settings.roots)
    {
        std::error_code error;
        if (std::filesystem::exists(root / path, error))
        {
            return root / path;
        }
    }
    return path;
}

void AssetManager::on_reloaded(std::type_index, AssetId)
{
}

void AssetManager::drain(const AssetBudget& budget)
{
    bool limited = budget.max_requests != 0 || budget.max_seconds > 0.0;
    do
    {
        m_executor->pump(budget);
        while (AssetRequest* request = m_executor->poll_completed())
        {
            request->publish();
            m_requests.erase(request);
        }
        while (!m_callbacks.empty())
        {
            std::vector<std::function<void()>> batch = std::move(m_callbacks);
            m_callbacks.clear();
            for (std::function<void()>& callback : batch)
            {
                callback();
            }
        }
    } while (!limited && !m_executor->idle());
}

void AssetManager::cancel(AssetRequest* request)
{
    if (request == nullptr)
    {
        return;
    }
    request->cancelled = true;
    if (m_executor->cancel(*request))
    {
        m_requests.erase(request);
    }
}

std::string AssetManager::path_key(const std::filesystem::path& path)
{
    return path.lexically_normal().generic_string();
}

std::string AssetManager::extension_key(const std::filesystem::path& path)
{
    std::string extension = path.extension().generic_string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension;
}

std::string AssetManager::describe(const Error& error)
{
    std::string text = error.what();
    if (!error.detail().empty())
    {
        text += ": " + error.detail();
    }
    return text;
}

} // namespace oryx
