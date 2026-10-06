#pragma once

#include "Oryx/Assets/Execution/AssetRequest.h"

namespace oryx
{

// Zero means unlimited. At least one request always runs per pump.
struct AssetBudget
{
    uint32_t max_requests = 0;
    double max_seconds = 0.0;
};

class IAssetExecutor
{
public:
    virtual ~IAssetExecutor() = default;

    virtual void submit(AssetRequest& request) = 0;
    // True when the request had not started; the executor then no longer references it.
    virtual bool cancel(AssetRequest& request) = 0;
    virtual void pump(const AssetBudget& budget) = 0;
    // Next request whose process() has finished, in submission order; nullptr when none.
    [[nodiscard]] virtual AssetRequest* poll_completed() = 0;
    [[nodiscard]] virtual bool idle() const = 0;
};

} // namespace oryx
