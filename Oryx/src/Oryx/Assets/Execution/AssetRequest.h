#pragma once

#include "Oryx/Core/Base.h"

namespace oryx
{

// process() touches only the request's own data so an executor may run it off the main thread; publish() always runs on the main thread.
class AssetRequest
{
public:
    virtual ~AssetRequest() = default;

    virtual void process() = 0;
    virtual void publish() = 0;

    bool cancelled = false;
};

} // namespace oryx
