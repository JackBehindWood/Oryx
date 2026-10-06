#pragma once

#include "Oryx/Graphics/RHI/Detail/RHIDebugScope.h"
#include "Oryx/Graphics/RHI/Detail/RHICommandListBase.h"
#include "Oryx/Graphics/RHI/Detail/RHIValidationContext.h"
#include "Oryx/Graphics/RHI/RHICommand.h"

namespace oryx
{

// Records and validates; every recording method throws Error on an invalid call.
// Commands live in the list's own arena and point at their resources; the list retains each resource when it is recorded, so the
// caller may drop its own references before IRHI::submit, which consumes the list and takes over the retained references.
class RHICommandList final : public RHICommandListBase
{
public:
    explicit RHICommandList(size_t initial_block_size = DEFAULT_BLOCK_SIZE)
        : RHICommandListBase(initial_block_size)
    {
    }
    ~RHICommandList() = default;
    RHICommandList(RHICommandList&& other) noexcept;
    RHICommandList& operator=(RHICommandList&& other) noexcept;

    void begin_pass(const RHIRenderPassDesc& pass);
    // Convenience: one colour attachment, cleared or loaded.
    void begin_pass(RHIRenderTarget* target, const RHIClear& clear = {});
    void set_pipeline(RHIGraphicsPipeline* pipeline);
    void set_viewport(const RHIViewportState& viewport);
    void set_scissor(const RHIScissorRect& scissor);
    void set_vertex_buffer(uint32_t slot, RHIBuffer* buffer, uint32_t offset = 0);
    void set_index_buffer(RHIBuffer* buffer, uint32_t offset = 0, bool index32 = true);
    // The bytes are copied into the list; at most RHI_MAX_CONSTANTS_SIZE and at most the binding's size.
    void set_constants(RHIBindingId binding, const void* data, uint32_t size);
    void bind_buffer(RHIBindingId binding, RHIBuffer* buffer, uint32_t offset, uint32_t size);
    void bind_texture(RHIBindingId binding, RHITexture* texture, uint32_t array_index = 0);
    void bind_sampler(RHIBindingId binding, RHISampler* sampler, uint32_t array_index = 0);
    // A draw needs every binding of the pipeline (each array element) and every vertex slot its attributes use bound since the last set_pipeline.
    void draw(uint32_t vertex_count, uint32_t instance_count = 1, uint32_t first_vertex = 0, uint32_t first_instance = 0);
    // The index range must lie inside the bound index buffer.
    void draw_indexed(uint32_t index_count, uint32_t instance_count = 1, uint32_t first_index = 0, int32_t base_vertex = 0, uint32_t first_instance = 0);
    // Groups must be balanced inside a pass and outside passes separately; the name is copied into the list.
    void push_debug_group(const char* name);
    void pop_debug_group();
    void end_pass();
    // Outside passes only. Copies on the GPU in recording order, so a CpuToGpu staging range written before submit can feed a GpuOnly buffer without blocking;
    // source needs CopySource, destination needs CopyDest, the buffers must differ and both ranges must fit.
    void copy_buffer(RHIBuffer* source, uint32_t source_offset, RHIBuffer* destination, uint32_t destination_offset, uint32_t size);

    // Drops all commands, retained resources and validation state.
    void clear();

    // For backends: moves the retained resources into `sink` and clears the list.
    void drain_into(std::vector<Ref<RHIResource>>& sink);

    [[nodiscard]] bool in_pass() const { return m_validation.in_pass(); }
    [[nodiscard]] uint32_t debug_depth() const { return m_validation.debug_depth(); }

private:
    RHIValidationContext m_validation;
};

} // namespace oryx
