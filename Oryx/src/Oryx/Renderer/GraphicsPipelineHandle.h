#pragma once

namespace oryx
{

// Names a pipeline owned by the renderer's GraphicsPipelineCache; stale once the cache is cleared.
struct GraphicsPipelineHandle
{
    uint32_t index = std::numeric_limits<uint32_t>::max();
    uint32_t generation = 0;
};

[[nodiscard]] constexpr bool graphics_pipeline_handle_valid(GraphicsPipelineHandle handle)
{
    return handle.generation != 0;
}

constexpr bool operator==(GraphicsPipelineHandle a, GraphicsPipelineHandle b)
{
    return a.index == b.index && a.generation == b.generation;
}

constexpr bool operator!=(GraphicsPipelineHandle a, GraphicsPipelineHandle b)
{
    return !(a == b);
}

} // namespace oryx
