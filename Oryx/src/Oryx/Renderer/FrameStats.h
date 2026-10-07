#pragma once

#include "Oryx/Renderer/Batch/BatchRenderer.h"

namespace oryx
{

inline constexpr uint32_t FRAME_STATS_PASSES = 8;

// Milliseconds of CPU the layer spent in each phase of one frame; filled by GraphicsLayer, zero for a frame recorded without one.
struct FrameCpuTimes
{
    float clients_ms = 0.0f;
    float scene_ms = 0.0f;
    float record_ms = 0.0f;
};

// Everything the renderer did in one finished frame, captured after the passes were recorded and before the batcher recycled.
struct FrameStats
{
    BatchStats batch;
    uint32_t passes = 0;
    uint32_t draw_items = 0;
    // Draw items per recorded pass in stage order; passes beyond the array fold into the last entry.
    uint32_t pass_items[FRAME_STATS_PASSES] = {};
    uint32_t pipeline_hits = 0;
    uint32_t pipeline_misses = 0;
    // Zero when no executable linked the allocation census.
    uint64_t allocations = 0;
    int64_t live_bytes = 0;
    bool presented = false;
    FrameCpuTimes cpu;
};

static_assert(std::is_trivially_copyable_v<FrameStats>);

} // namespace oryx
