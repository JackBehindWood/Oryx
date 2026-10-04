#pragma once

#include "MetalApi.h"
#include "MetalDevice.h"
#include "Oryx/Graphics/RHI/RHIResource.h"

namespace oryx::metal
{

// Frame pacing on the application thread only: a ring of METAL_FRAMES_IN_FLIGHT frames, each owning its committed command buffers and the
// resources they use. Metal callback threads never touch this state; a failed command buffer throws from the next frame boundary or poll.
class MetalCommandQueue
{
public:
    MetalCommandQueue(MetalDevice& device, RHIDeviceLease& lease);

    [[nodiscard]] NS::SharedPtr<MTL::CommandBuffer> make_command_buffer();
    // Commits and attaches the command buffer to the current frame.
    void commit(NS::SharedPtr<MTL::CommandBuffer> commands);
    [[nodiscard]] std::vector<Ref<RHIResource>>& frame_refs() { return m_frames[m_current].refs; }

    [[nodiscard]] uint32_t current_frame() const { return static_cast<uint32_t>(m_current); }

    // Ends the current frame; blocks while the oldest frame is still in flight so at most METAL_FRAMES_IN_FLIGHT overlap.
    void end_frame();
    // Retires every finished frame without blocking.
    void poll();
    // Blocks until the GPU is done with everything and nothing is in flight.
    void wait_idle();

private:
    struct Frame
    {
        uint64_t serial = 0;
        bool ended = false;
        std::vector<NS::SharedPtr<MTL::CommandBuffer>> commands;
        std::vector<Ref<RHIResource>> refs;
    };

    [[nodiscard]] static bool finished(const Frame& frame);
    [[nodiscard]] std::string retire(Frame& frame, bool wait);
    void sync_lease();

    MetalDevice& m_device;
    RHIDeviceLease& m_lease;
    std::vector<Frame> m_frames;
    size_t m_current = 0;
};

} // namespace oryx::metal
