#pragma once

#include "Oryx/Graphics/RHI/RHICommandContext.h"
#include "Oryx/Graphics/RHI/RHIRenderState.h"

namespace oryx
{

// Checks every call against the state the previous calls built (pass, pipeline, bound resources, debug scopes) and throws Error on a mistake.
// With an inner context it forwards each valid call, so it can wrap a backend to check a hand-built or replayed stream; the caller keeps the resources and debug names alive.
// RHICommandList owns one without an inner context, so mistakes throw at the recording call.
class RHIValidationContext final : public IRHICommandContext
{
public:
    static constexpr uint32_t MAX_BREADCRUMBS = 16;

    explicit RHIValidationContext(IRHICommandContext* inner = nullptr)
        : m_inner(inner)
    {
    }

    void begin_pass(const RHIRenderPassDesc& pass) override;
    void set_pipeline(RHIGraphicsPipeline& pipeline) override;
    void set_viewport(const RHIViewportState& viewport) override;
    void set_scissor(const RHIScissorRect& scissor) override;
    void set_vertex_buffer(uint32_t slot, RHIBuffer& buffer, uint32_t offset) override;
    void set_index_buffer(RHIBuffer& buffer, uint32_t offset, bool index32) override;
    void set_constants(RHIBindingId binding, const uint8_t* data, uint32_t size) override;
    void bind_buffer(RHIBindingId binding, RHIBuffer& buffer, uint32_t offset, uint32_t size) override;
    void bind_texture(RHIBindingId binding, RHITexture& texture, uint32_t array_index) override;
    void bind_sampler(RHIBindingId binding, RHISampler& sampler, uint32_t array_index) override;
    void draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) override;
    void draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t base_vertex, uint32_t first_instance) override;
    void push_debug_group(const char* name) override;
    void pop_debug_group() override;
    void end_pass() override;
    void copy_buffer(RHIBuffer& source, uint32_t source_offset, RHIBuffer& destination, uint32_t destination_offset, uint32_t size) override;

    // Forgets all state, as for a new recording.
    void reset();

    [[nodiscard]] bool in_pass() const { return m_in_pass; }
    [[nodiscard]] uint32_t debug_depth() const { return m_debug_depth; }
    // The open pass's attachment size; zero outside a pass.
    [[nodiscard]] RHIScissorRect pass_extent() const { return { 0, 0, m_pass_width, m_pass_height }; }
    // The open debug groups, outermost first, joined with " > "; empty when none are open.
    [[nodiscard]] std::string breadcrumb_path() const;

private:
    [[noreturn]] void fail(const char* what, const std::string& message) const;
    void require_pass(const char* what) const;
    void require_pipeline(const char* what) const;
    void require_draw_ready(const char* what) const;
    [[nodiscard]] const RHIBindingDesc& require_binding(RHIBindingId binding, const char* what) const;
    void mark_bound(RHIBindingId binding, uint32_t array_index);

    IRHICommandContext* m_inner;
    const RHIGraphicsPipeline* m_pipeline = nullptr;
    RHIFormat m_pass_colour[RHI_MAX_COLOUR_TARGETS] = {};
    RHIFormat m_pass_depth = RHIFormat::Undefined;
    uint32_t m_pass_colour_count = 0;
    uint32_t m_pass_width = 0;
    uint32_t m_pass_height = 0;
    uint32_t m_debug_depth = 0;
    uint32_t m_pass_debug_base = 0;
    bool m_in_pass = false;
    bool m_has_index_buffer = false;
    bool m_index32 = true;
    uint32_t m_index_offset = 0;
    uint32_t m_index_buffer_size = 0;
    uint32_t m_vertex_slots_bound = 0;
    uint32_t m_bound_elements[RHI_MAX_BINDINGS] = {};
    const char* m_breadcrumbs[MAX_BREADCRUMBS] = {};
};

} // namespace oryx
