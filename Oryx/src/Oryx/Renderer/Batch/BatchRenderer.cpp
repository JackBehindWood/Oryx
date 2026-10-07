#include "oxpch.h"
#include "Oryx/Renderer/Batch/BatchRenderer.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Shaders/Builtin/BuiltinShaders.h"

namespace oryx
{

namespace
{

constexpr uint32_t VERTEX_ALIGNMENT = 16;

uint32_t texture_permutation(const BatchRendererDesc& desc)
{
    return TextureArrayPermutations::fitting(desc.max_texture_bindings != 0 ? desc.max_texture_bindings : desc.rhi.capabilities().max_texture_bindings);
}

uint32_t align_up(uint32_t value, uint32_t alignment)
{
    return (value + alignment - 1) / alignment * alignment;
}

} // namespace

BatchStats batch_stats_delta(const BatchStats& after, const BatchStats& before)
{
    BatchStats delta = after;
    delta.draws -= before.draws;
    delta.primitives -= before.primitives;
    delta.vertices -= before.vertices;
    delta.triangles -= before.triangles;
    delta.bytes -= before.bytes;
    for (uint32_t reason = 0; reason < FLUSH_REASON_COUNT; ++reason)
    {
        delta.flushes[reason] -= before.flushes[reason];
    }
    for (uint32_t stream = 0; stream < BATCH_MAX_STREAMS; ++stream)
    {
        delta.streams[stream].draws -= before.streams[stream].draws;
        delta.streams[stream].primitives -= before.streams[stream].primitives;
        delta.streams[stream].vertices -= before.streams[stream].vertices;
        delta.streams[stream].bytes -= before.streams[stream].bytes;
        for (uint32_t to = 0; to < BATCH_MAX_STREAMS; ++to)
        {
            delta.stream_switches[stream][to] -= before.stream_switches[stream][to];
        }
    }
    return delta;
}

BatchRenderer::BatchRenderer(const BatchRendererDesc& desc)
    : m_rhi(desc.rhi)
    , m_pipelines(desc.pipelines)
    , m_memo(desc.memo)
    , m_shaders(desc.shaders)
    , m_defaults(desc.defaults)
    , m_default_sink(desc.sink)
    , m_sink(&desc.sink)
    , m_slots(desc.defaults.white_texture, TextureArrayPermutations::value(texture_permutation(desc)))
    , m_default_formats{ desc.colour_format, RHIFormat::Undefined }
    , m_formats(m_default_formats)
    , m_page_bytes(align_up(desc.page_bytes, TransientAllocator::REGION_ALIGNMENT))
    , m_max_indexed_primitives(desc.max_indexed_primitives)
    , m_permutation(texture_permutation(desc))
{
    if (desc.page_bytes == 0 || desc.max_indexed_primitives == 0 || desc.max_indexed_primitives > QUAD_INDEX_MAX_QUADS)
    {
        throw Error("BatchRendererDesc is invalid", "page_bytes must be positive and max_indexed_primitives within 1.." + std::to_string(QUAD_INDEX_MAX_QUADS));
    }
}

BatchStreamId BatchRenderer::register_stream(const BatchStreamDesc& stream)
{
    const bool indexed = stream.indices_per_primitive != 0;
    if (stream.vertex_size == 0 || stream.vertices_per_primitive == 0 || (indexed && (stream.vertices_per_primitive != 4 || stream.indices_per_primitive != 6)))
    {
        throw Error("BatchStreamDesc is invalid", "indexed streams must be quads (4 vertices, 6 indices)");
    }
    if (m_streams.size() >= BATCH_MAX_STREAMS)
    {
        throw Error("BatchRenderer has too many streams");
    }
    m_streams.push_back(stream);
    return static_cast<BatchStreamId>(m_streams.size() - 1);
}

void BatchRenderer::require_open() const
{
    if (!m_open)
    {
        throw Error("BatchRenderer has no open scene", "call begin first");
    }
}

void BatchRenderer::begin(const Camera& camera)
{
    begin(camera, BatchTarget{ m_default_sink, m_default_formats });
}

void BatchRenderer::begin(const Camera& camera, const BatchTarget& target)
{
    if (m_open)
    {
        throw Error("BatchRenderer scene is already open", "call end first");
    }
    m_sink = &target.sink;
    m_formats = target.formats;
    camera.to_gpu(m_constants.view_projection);
    m_view_projection = camera.view_projection();
    m_framebuffer = target.framebuffer;
    m_has_scissor = false;
    m_staging.clear();
    m_slots.reset();
    m_sampler.reset();
    m_primitives = 0;
    m_open = true;
    on_begin();
}

void BatchRenderer::set_scissor(const RHIScissorRect& scissor)
{
    require_open();
    const bool same = m_has_scissor && m_scissor.x == scissor.x && m_scissor.y == scissor.y && m_scissor.width == scissor.width && m_scissor.height == scissor.height;
    if (same)
    {
        return;
    }
    flush_batch(FlushReason::ScissorChange);
    m_scissor = scissor;
    m_has_scissor = true;
}

void BatchRenderer::clear_scissor()
{
    require_open();
    if (!m_has_scissor)
    {
        return;
    }
    flush_batch(FlushReason::ScissorChange);
    m_has_scissor = false;
}

void BatchRenderer::flush(FlushReason reason)
{
    require_open();
    flush_batch(reason);
}

void BatchRenderer::end()
{
    require_open();
    flush_batch(FlushReason::End);
    m_open = false;
}

void BatchRenderer::recycle(uint32_t frame_slot)
{
    for (TransientAllocator& page : m_pages)
    {
        page.reset(frame_slot);
    }
    m_page = 0;
    m_stats = {};
    m_stats.pages = static_cast<uint32_t>(m_pages.size());
}

void BatchRenderer::select(BatchStreamId stream, const RHISamplerPtr& sampler)
{
    require_open();
    if (stream >= m_streams.size())
    {
        throw Error("BatchRenderer stream is not registered");
    }
    if (m_primitives > 0)
    {
        if (stream != m_stream)
        {
            ++m_stats.stream_switches[m_stream][stream];
            flush_batch(FlushReason::StreamChange);
        }
        else if (sampler != m_sampler)
        {
            flush_batch(FlushReason::SamplerChange);
        }
        else if (m_streams[stream].indices_per_primitive != 0 && m_primitives >= m_max_indexed_primitives)
        {
            flush_batch(FlushReason::IndexLimit);
        }
    }
    m_stream = stream;
    m_sampler = sampler;
}

uint32_t BatchRenderer::acquire_texture(const RHITexturePtr& texture)
{
    uint32_t slot = m_slots.acquire(texture);
    if (slot == TextureSlotTable::FULL)
    {
        flush_batch(FlushReason::TextureSlotsFull);
        slot = m_slots.acquire(texture);
    }
    return slot;
}

uint8_t* BatchRenderer::append()
{
    const BatchStreamDesc& stream = m_streams[m_stream];
    const size_t first = m_staging.size();
    m_staging.resize(first + static_cast<size_t>(stream.vertex_size) * stream.vertices_per_primitive);
    ++m_primitives;
    return m_staging.data() + first;
}

TransientAllocation BatchRenderer::allocate(uint32_t bytes)
{
    const uint32_t frame_slot = m_rhi.frame_slot();
    for (; m_page < m_pages.size(); ++m_page)
    {
        TransientAllocator& page = m_pages[m_page];
        const uint32_t start = align_up(page.used(frame_slot), VERTEX_ALIGNMENT);
        if (start <= page.capacity() && bytes <= page.capacity() - start)
        {
            return page.allocate(frame_slot, bytes, VERTEX_ALIGNMENT);
        }
    }
    m_pages.push_back(TransientAllocator::create(m_rhi, std::max(m_page_bytes, align_up(bytes, TransientAllocator::REGION_ALIGNMENT)), RHIBufferUsage::Vertex));
    m_stats.pages = static_cast<uint32_t>(m_pages.size());
    return m_pages.back().allocate(frame_slot, bytes, VERTEX_ALIGNMENT);
}

void BatchRenderer::flush_batch(FlushReason reason)
{
    const uint32_t primitives = m_primitives;
    const uint32_t bytes = static_cast<uint32_t>(m_staging.size());
    if (primitives > 0)
    {
        const BatchStreamDesc& stream = m_streams[m_stream];
        const TransientAllocation allocation = allocate(bytes);
        std::memcpy(allocation.data, m_staging.data(), bytes);

        DrawItem item;
        item.pipeline = m_memo.get(m_rhi, m_pipelines, m_shaders, m_formats, *stream.pipeline, stream.textured ? m_permutation : 0);
        item.vertex_buffers[0] = allocation.buffer;
        item.vertex_offsets[0] = allocation.offset;
        item.vertex_count = primitives * stream.vertices_per_primitive;
        if (stream.indices_per_primitive != 0)
        {
            item.index_buffer = m_defaults.quad_indices;
            item.index_type = IndexType::U16;
            item.index_count = primitives * stream.indices_per_primitive;
        }
        draw_item_set_constants(item, m_constants);
        if (stream.textured)
        {
            for (uint32_t i = 0; i < m_slots.count(); ++i)
            {
                draw_item_add_texture(item, m_slots.texture(i));
            }
        }
        item.sampler = m_sampler;
        item.scissor = m_scissor;
        item.has_scissor = m_has_scissor;
        m_sink->push_back(std::move(item));

        ++m_stats.draws;
        m_stats.primitives += primitives;
        m_stats.vertices += primitives * stream.vertices_per_primitive;
        m_stats.triangles += primitives * stream.triangles_per_primitive;
        if (stream.textured)
        {
            m_stats.texture_slots_used = std::max(m_stats.texture_slots_used, m_slots.count());
        }
        m_stats.bytes += bytes;
        BatchStreamStats& per_stream = m_stats.streams[m_stream];
        ++per_stream.draws;
        per_stream.primitives += primitives;
        per_stream.vertices += primitives * stream.vertices_per_primitive;
        per_stream.bytes += bytes;
        ++m_stats.flushes[static_cast<uint32_t>(reason)];
    }
    m_staging.clear();
    m_slots.reset();
    m_primitives = 0;
}

} // namespace oryx
