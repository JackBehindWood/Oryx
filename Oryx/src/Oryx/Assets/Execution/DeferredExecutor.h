#pragma once

#include "Oryx/Assets/Execution/IAssetExecutor.h"

namespace oryx
{

// Runs queued requests inside pump() on the calling thread, first in first out.
class DeferredExecutor : public IAssetExecutor
{
public:
    void submit(AssetRequest& request) override;
    bool cancel(AssetRequest& request) override;
    void pump(const AssetBudget& budget) override;
    AssetRequest* poll_completed() override;
    [[nodiscard]] bool idle() const override;

private:
    std::deque<AssetRequest*> m_queued;
    std::deque<AssetRequest*> m_completed;
};

} // namespace oryx
