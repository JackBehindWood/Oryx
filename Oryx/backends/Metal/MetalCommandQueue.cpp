#include "oxpch.h"
#include "MetalCommandQueue.h"

#include "MetalConvert.h"
#include "Oryx/Core/Error.h"

namespace oryx::metal
{

MetalCommandQueue::MetalCommandQueue(MetalDevice& device, RHIDeviceLease& lease)
    : m_device(device)
    , m_lease(lease)
    , m_frames(METAL_FRAMES_IN_FLIGHT)
{
}

NS::SharedPtr<MTL::CommandBuffer> MetalCommandQueue::make_command_buffer()
{
    return require_object(NS::RetainPtr(m_device.command_queue()->commandBuffer()), "a command buffer");
}

void MetalCommandQueue::commit(NS::SharedPtr<MTL::CommandBuffer> commands)
{
    commands->commit();
    m_lease.begin_work();
    m_frames[m_current].commands.push_back(std::move(commands));
}

bool MetalCommandQueue::finished(const Frame& frame)
{
    for (const NS::SharedPtr<MTL::CommandBuffer>& commands : frame.commands)
    {
        const MTL::CommandBufferStatus status = commands->status();
        if (status != MTL::CommandBufferStatusCompleted && status != MTL::CommandBufferStatusError)
        {
            return false;
        }
    }
    return true;
}

std::string MetalCommandQueue::retire(Frame& frame, bool wait)
{
    std::string failure;
    for (const NS::SharedPtr<MTL::CommandBuffer>& commands : frame.commands)
    {
        if (wait)
        {
            commands->waitUntilCompleted();
        }
        if (failure.empty())
        {
            failure = command_buffer_failure(*commands.get());
        }
    }
    const bool completes_serial = frame.ended;
    const uint64_t serial = frame.serial;
    frame.commands.clear();
    frame.refs.clear();
    frame.ended = false;
    if (completes_serial)
    {
        m_lease.complete(serial);
    }
    sync_lease();
    return failure;
}

void MetalCommandQueue::sync_lease()
{
    for (const Frame& frame : m_frames)
    {
        if (!frame.commands.empty())
        {
            return;
        }
    }
    m_lease.set_idle();
}

void MetalCommandQueue::end_frame()
{
    Frame& ended = m_frames[m_current];
    ended.serial = m_lease.end_frame();
    ended.ended = true;
    m_current = (m_current + 1) % m_frames.size();

    Frame& oldest = m_frames[m_current];
    if (oldest.ended)
    {
        std::string failure = retire(oldest, true);
        if (!failure.empty())
        {
            throw Error("Metal command buffer failed", failure);
        }
    }
    poll();
}

void MetalCommandQueue::poll()
{
    for (size_t offset = 1; offset < m_frames.size(); ++offset)
    {
        Frame& frame = m_frames[(m_current + offset) % m_frames.size()];
        if (!frame.ended)
        {
            continue;
        }
        if (!finished(frame))
        {
            return;
        }
        std::string failure = retire(frame, false);
        if (!failure.empty())
        {
            throw Error("Metal command buffer failed", failure);
        }
    }
}

void MetalCommandQueue::wait_idle()
{
    std::string failure;
    for (size_t offset = 1; offset <= m_frames.size(); ++offset)
    {
        std::string frame_failure = retire(m_frames[(m_current + offset) % m_frames.size()], true);
        if (failure.empty())
        {
            failure = std::move(frame_failure);
        }
    }
    m_lease.end_frame();
    m_lease.set_idle();
    if (!failure.empty())
    {
        throw Error("Metal command buffer failed", failure);
    }
}

} // namespace oryx::metal
