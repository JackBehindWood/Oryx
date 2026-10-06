#pragma once

#include "Oryx/Graphics/RHI/Detail/RHICommandStream.h"
#include "Oryx/Graphics/RHI/Detail/RHIResourceRetainer.h"

namespace oryx
{

// Command memory and resource bookkeeping shared by every kind of recording list; the recording API lives in the derived classes.
class RHICommandListBase
{
public:
    using Iterator = RHICommandStream::Iterator;

    static constexpr size_t DEFAULT_BLOCK_SIZE = RHICommandStream::DEFAULT_BLOCK_SIZE;

    RHICommandListBase(const RHICommandListBase&) = delete;
    RHICommandListBase& operator=(const RHICommandListBase&) = delete;

    // Replays every command into the backend's context, in recording order; a plain Error from a command gains its index and type.
    void execute(IRHICommandContext& context) const;

    // Drops all commands and releases the retained resources; the arena's memory is kept for reuse.
    void clear();

    // Moves the retained resources into `sink` and clears the commands.
    void drain_into(std::vector<Ref<RHIResource>>& sink);

    [[nodiscard]] Iterator begin() const { return m_stream.begin(); }
    [[nodiscard]] Iterator end() const { return m_stream.end(); }
    [[nodiscard]] size_t size() const { return m_stream.size(); }
    [[nodiscard]] bool empty() const { return m_stream.size() == 0; }
    [[nodiscard]] size_t retained_count() const { return m_retainer.size(); }

protected:
    explicit RHICommandListBase(size_t block_size)
        : m_stream(block_size)
    {
    }

    ~RHICommandListBase() = default;
    RHICommandListBase(RHICommandListBase&&) noexcept = default;
    RHICommandListBase& operator=(RHICommandListBase&&) noexcept = default;

    RHICommandStream m_stream;
    RHIResourceRetainer m_retainer;
};

} // namespace oryx
