#pragma once

#include "Oryx/Graphics/RHI/IRHI.h"
#include "Oryx/Graphics/Resources/TransientAllocator.h"
#include "Oryx/Renderer/BuiltinPipelines.h"
#include "Oryx/Renderer/Camera.h"
#include "Oryx/Renderer/DefaultResources.h"
#include "Oryx/Renderer/DrawItem.h"
#include "Oryx/Renderer/TextureSlotTable.h"

namespace oryx
{

enum class FlushReason : uint8_t
{
    Explicit,
    End,
    StreamChange,
    SamplerChange,
    TextureSlotsFull,
    IndexLimit
};

inline constexpr uint32_t FLUSH_REASON_COUNT = 6;

struct BatchStats
{
    uint32_t draws = 0;
    uint32_t primitives = 0;
    uint32_t vertices = 0;
    uint32_t bytes = 0;
    uint32_t pages = 0;
    uint32_t flushes[FLUSH_REASON_COUNT] = {};
};

// Frame constants every batched pipeline reads at SHADER_FRAME_BINDING.
struct BatchConstants
{
    float view_projection[16];
};

// One kind of primitive a batcher can draw. Indexed streams draw quads (4 vertices, 6 indices) from the shared quad index buffer.
struct BatchStreamDesc
{
    uint32_t vertex_size = 0;
    uint32_t vertices_per_primitive = 0;
    uint32_t indices_per_primitive = 0;
    bool textured = false;
    BuiltinPipeline pipeline = BuiltinPipeline::Quad;
};

using BatchStreamId = uint8_t;

inline constexpr uint32_t BATCH_DEFAULT_PAGE_BYTES = 4 * 1024 * 1024;

struct BatchRendererDesc
{
    IRHI& rhi;
    GraphicsPipelineCache& pipelines;
    BuiltinPipelines& builtin;
    const ShaderLibrary& shaders;
    const DefaultResources& defaults;
    std::vector<DrawItem>& sink;
    RHIFormat colour_format = RHIFormat::BGRA8Unorm;
    uint32_t page_bytes = BATCH_DEFAULT_PAGE_BYTES;
    uint32_t max_indexed_primitives = QUAD_INDEX_MAX_QUADS;
    // 0 reads the device's capabilities; a smaller value forces the 16-slot permutation.
    uint32_t max_texture_bindings = 0;
};

// Accumulates transient primitives on the CPU and turns each run of identical state into one DrawItem pushed to the sink, in submission order.
// An ordinary instance class: scenes may nest no deeper than one begin/end, but any number of batchers may share a sink.
class BatchRenderer
{
public:
    explicit BatchRenderer(const BatchRendererDesc& desc);

    BatchRenderer(const BatchRenderer&) = delete;
    BatchRenderer& operator=(const BatchRenderer&) = delete;

    // Throws Error if a scene is already open.
    void begin(const Camera& camera);
    // Throws Error outside a scene.
    void flush(FlushReason reason = FlushReason::Explicit);
    void end();
    // Reuses the transient pages of `frame_slot`; the owner calls it when that slot's frame starts, never between scenes of one frame.
    void recycle(uint32_t frame_slot);

    [[nodiscard]] bool open() const { return m_open; }
    [[nodiscard]] const BatchStats& stats() const { return m_stats; }
    [[nodiscard]] uint32_t texture_slot_count() const { return m_slots.capacity(); }

protected:
    BatchStreamId register_stream(const BatchStreamDesc& stream);

    // Primitives are added as select -> acquire_texture (textured streams) -> append; only the first two may flush, so a slot index stays valid until append.
    void select(BatchStreamId stream, const RHISamplerPtr& sampler);
    [[nodiscard]] uint32_t acquire_texture(const RHITexturePtr& texture);
    // Space for one primitive of the selected stream; valid until the next call into the batcher.
    [[nodiscard]] uint8_t* append();

    [[nodiscard]] const DefaultResources& defaults() const { return m_defaults; }

private:
    void flush_batch(FlushReason reason);
    [[nodiscard]] TransientAllocation allocate(uint32_t bytes);
    void require_open() const;

    IRHI& m_rhi;
    GraphicsPipelineCache& m_pipelines;
    BuiltinPipelines& m_builtin;
    const ShaderLibrary& m_shaders;
    const DefaultResources& m_defaults;
    std::vector<DrawItem>& m_sink;
    std::vector<BatchStreamDesc> m_streams;
    std::vector<TransientAllocator> m_pages;
    std::vector<uint8_t> m_staging;
    TextureSlotTable m_slots;
    BatchConstants m_constants = {};
    BatchStats m_stats;
    RHISamplerPtr m_sampler;
    RHIFormat m_format;
    uint32_t m_page_bytes;
    uint32_t m_page = 0;
    uint32_t m_max_indexed_primitives;
    uint32_t m_permutation;
    uint32_t m_primitives = 0;
    BatchStreamId m_stream = 0;
    bool m_open = false;
};

} // namespace oryx
