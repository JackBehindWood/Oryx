#pragma once

#include "Oryx/Memory/RefCounted.h"

namespace oryx
{

// The last release defers destruction until every device has finished the frame the resource was last used in (see RHIDeviceLease).
// A resource must not outlive its device.
class RHIResource : public RefCounted
{
public:
    ~RHIResource() override;

    [[nodiscard]] static size_t live_count();
    // The live resources grouped by type, "3 x NullBuffer, 1 x NullTexture"; empty when none are live.
    [[nodiscard]] static std::string live_report();
    [[nodiscard]] static size_t retired_pending();
    [[nodiscard]] static uint64_t frame_serial();

protected:
    RHIResource();

private:
    void on_last_release() noexcept override;
};

// One per device, constructed by IRHI implementations only. Retired resources are freed once their frame is complete on every live device; an idle device never blocks that.
class RHIDeviceLease
{
public:
    RHIDeviceLease();
    ~RHIDeviceLease();
    RHIDeviceLease(const RHIDeviceLease&) = delete;
    RHIDeviceLease& operator=(const RHIDeviceLease&) = delete;

    // Frame boundary: returns the serial of the frame that just ended.
    uint64_t end_frame();
    // This device has work in flight and has completed no frame yet; a no-op once busy.
    void begin_work();
    // This device has finished every frame up to and including `serial`; collects what is now safe.
    void complete(uint64_t serial);
    // Nothing in flight on this device; collects what is now safe.
    void set_idle();

private:
    friend struct RHIRetireState;
    // Exclusive bound: frames with a serial below it are complete on this device.
    uint64_t m_completed;
};

} // namespace oryx
