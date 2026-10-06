#include "oxpch.h"
#include "Oryx/Assets/Execution/DeferredExecutor.h"

namespace oryx
{

void DeferredExecutor::submit(AssetRequest& request)
{
    m_queued.push_back(&request);
}

bool DeferredExecutor::cancel(AssetRequest& request)
{
    std::deque<AssetRequest*>::iterator it = std::find(m_queued.begin(), m_queued.end(), &request);
    if (it == m_queued.end())
    {
        return false;
    }
    m_queued.erase(it);
    return true;
}

void DeferredExecutor::pump(const AssetBudget& budget)
{
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    uint32_t processed = 0;
    while (!m_queued.empty())
    {
        if (processed > 0)
        {
            bool out_of_count = budget.max_requests != 0 && processed >= budget.max_requests;
            bool out_of_time = budget.max_seconds > 0.0 && std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() >= budget.max_seconds;
            if (out_of_count || out_of_time)
            {
                break;
            }
        }
        AssetRequest* request = m_queued.front();
        m_queued.pop_front();
        request->process();
        m_completed.push_back(request);
        ++processed;
    }
}

AssetRequest* DeferredExecutor::poll_completed()
{
    if (m_completed.empty())
    {
        return nullptr;
    }
    AssetRequest* request = m_completed.front();
    m_completed.pop_front();
    return request;
}

bool DeferredExecutor::idle() const
{
    return m_queued.empty() && m_completed.empty();
}

} // namespace oryx
