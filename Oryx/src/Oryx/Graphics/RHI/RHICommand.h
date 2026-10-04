#pragma once

#include "Oryx/Graphics/RHI/RHICommandContext.h"

namespace oryx
{

enum class RHICommandType : uint8_t
{
    BeginPass,
    SetPipeline,
    SetViewport,
    SetScissor,
    SetVertexBuffer,
    SetIndexBuffer,
    SetConstants,
    BindBuffer,
    BindTexture,
    BindSampler,
    Draw,
    DrawIndexed,
    PushDebugGroup,
    PopDebugGroup,
    EndPass
};

class RHICommandList;

// Commands are arena-allocated by the list and never destroyed individually, so they must stay trivially destructible and hold raw pointers.
class RHICommand
{
public:
    RHICommand(const RHICommand&) = delete;
    RHICommand& operator=(const RHICommand&) = delete;

    [[nodiscard]] RHICommandType type() const { return m_type; }
    [[nodiscard]] const RHICommand* next() const { return m_next; }

    virtual void execute(IRHICommandContext& context) const = 0;

protected:
    explicit RHICommand(RHICommandType type)
        : m_type(type)
    {
    }

    ~RHICommand() = default;

private:
    friend class RHICommandList;

    RHICommandType m_type;
    RHICommand* m_next = nullptr;
};

class RHIBeginPassCommand final : public RHICommand
{
public:
    static constexpr RHICommandType TYPE = RHICommandType::BeginPass;

    explicit RHIBeginPassCommand(const RHIRenderPassDesc& pass)
        : RHICommand(TYPE)
        , m_pass(pass)
    {
    }

    [[nodiscard]] const RHIRenderPassDesc& pass() const { return m_pass; }

    void execute(IRHICommandContext& context) const override { context.begin_pass(m_pass); }

private:
    RHIRenderPassDesc m_pass;
};

class RHISetPipelineCommand final : public RHICommand
{
public:
    static constexpr RHICommandType TYPE = RHICommandType::SetPipeline;

    explicit RHISetPipelineCommand(RHIGraphicsPipeline& pipeline)
        : RHICommand(TYPE)
        , m_pipeline(&pipeline)
    {
    }

    [[nodiscard]] RHIGraphicsPipeline& pipeline() const { return *m_pipeline; }

    void execute(IRHICommandContext& context) const override { context.set_pipeline(*m_pipeline); }

private:
    RHIGraphicsPipeline* m_pipeline;
};

class RHISetViewportCommand final : public RHICommand
{
public:
    static constexpr RHICommandType TYPE = RHICommandType::SetViewport;

    explicit RHISetViewportCommand(const RHIViewportState& viewport)
        : RHICommand(TYPE)
        , m_viewport(viewport)
    {
    }

    [[nodiscard]] const RHIViewportState& viewport() const { return m_viewport; }

    void execute(IRHICommandContext& context) const override { context.set_viewport(m_viewport); }

private:
    RHIViewportState m_viewport;
};

class RHISetScissorCommand final : public RHICommand
{
public:
    static constexpr RHICommandType TYPE = RHICommandType::SetScissor;

    explicit RHISetScissorCommand(const RHIScissorRect& scissor)
        : RHICommand(TYPE)
        , m_scissor(scissor)
    {
    }

    [[nodiscard]] const RHIScissorRect& scissor() const { return m_scissor; }

    void execute(IRHICommandContext& context) const override { context.set_scissor(m_scissor); }

private:
    RHIScissorRect m_scissor;
};

class RHISetVertexBufferCommand final : public RHICommand
{
public:
    static constexpr RHICommandType TYPE = RHICommandType::SetVertexBuffer;

    RHISetVertexBufferCommand(uint32_t slot, RHIBuffer& buffer, uint32_t offset)
        : RHICommand(TYPE)
        , m_slot(slot)
        , m_buffer(&buffer)
        , m_offset(offset)
    {
    }

    [[nodiscard]] uint32_t slot() const { return m_slot; }
    [[nodiscard]] RHIBuffer& buffer() const { return *m_buffer; }
    [[nodiscard]] uint32_t offset() const { return m_offset; }

    void execute(IRHICommandContext& context) const override { context.set_vertex_buffer(m_slot, *m_buffer, m_offset); }

private:
    uint32_t m_slot;
    RHIBuffer* m_buffer;
    uint32_t m_offset;
};

class RHISetIndexBufferCommand final : public RHICommand
{
public:
    static constexpr RHICommandType TYPE = RHICommandType::SetIndexBuffer;

    RHISetIndexBufferCommand(RHIBuffer& buffer, uint32_t offset, bool index32)
        : RHICommand(TYPE)
        , m_buffer(&buffer)
        , m_offset(offset)
        , m_index32(index32)
    {
    }

    [[nodiscard]] RHIBuffer& buffer() const { return *m_buffer; }
    [[nodiscard]] uint32_t offset() const { return m_offset; }
    [[nodiscard]] bool index32() const { return m_index32; }

    void execute(IRHICommandContext& context) const override { context.set_index_buffer(*m_buffer, m_offset, m_index32); }

private:
    RHIBuffer* m_buffer;
    uint32_t m_offset;
    bool m_index32;
};

// `data` points into the recording list's arena.
class RHISetConstantsCommand final : public RHICommand
{
public:
    static constexpr RHICommandType TYPE = RHICommandType::SetConstants;

    RHISetConstantsCommand(RHIBindingId binding, const uint8_t* data, uint32_t size)
        : RHICommand(TYPE)
        , m_binding(binding)
        , m_data(data)
        , m_size(size)
    {
    }

    [[nodiscard]] RHIBindingId binding() const { return m_binding; }
    [[nodiscard]] const uint8_t* data() const { return m_data; }
    [[nodiscard]] uint32_t size() const { return m_size; }

    void execute(IRHICommandContext& context) const override { context.set_constants(m_binding, m_data, m_size); }

private:
    RHIBindingId m_binding;
    const uint8_t* m_data;
    uint32_t m_size;
};

class RHIBindBufferCommand final : public RHICommand
{
public:
    static constexpr RHICommandType TYPE = RHICommandType::BindBuffer;

    RHIBindBufferCommand(RHIBindingId binding, RHIBuffer& buffer, uint32_t offset, uint32_t size)
        : RHICommand(TYPE)
        , m_binding(binding)
        , m_buffer(&buffer)
        , m_offset(offset)
        , m_size(size)
    {
    }

    [[nodiscard]] RHIBindingId binding() const { return m_binding; }
    [[nodiscard]] RHIBuffer& buffer() const { return *m_buffer; }
    [[nodiscard]] uint32_t offset() const { return m_offset; }
    [[nodiscard]] uint32_t size() const { return m_size; }

    void execute(IRHICommandContext& context) const override { context.bind_buffer(m_binding, *m_buffer, m_offset, m_size); }

private:
    RHIBindingId m_binding;
    RHIBuffer* m_buffer;
    uint32_t m_offset;
    uint32_t m_size;
};

class RHIBindTextureCommand final : public RHICommand
{
public:
    static constexpr RHICommandType TYPE = RHICommandType::BindTexture;

    RHIBindTextureCommand(RHIBindingId binding, RHITexture& texture, uint32_t array_index)
        : RHICommand(TYPE)
        , m_binding(binding)
        , m_texture(&texture)
        , m_array_index(array_index)
    {
    }

    [[nodiscard]] RHIBindingId binding() const { return m_binding; }
    [[nodiscard]] RHITexture& texture() const { return *m_texture; }
    [[nodiscard]] uint32_t array_index() const { return m_array_index; }

    void execute(IRHICommandContext& context) const override { context.bind_texture(m_binding, *m_texture, m_array_index); }

private:
    RHIBindingId m_binding;
    RHITexture* m_texture;
    uint32_t m_array_index;
};

class RHIBindSamplerCommand final : public RHICommand
{
public:
    static constexpr RHICommandType TYPE = RHICommandType::BindSampler;

    RHIBindSamplerCommand(RHIBindingId binding, RHISampler& sampler, uint32_t array_index)
        : RHICommand(TYPE)
        , m_binding(binding)
        , m_sampler(&sampler)
        , m_array_index(array_index)
    {
    }

    [[nodiscard]] RHIBindingId binding() const { return m_binding; }
    [[nodiscard]] RHISampler& sampler() const { return *m_sampler; }
    [[nodiscard]] uint32_t array_index() const { return m_array_index; }

    void execute(IRHICommandContext& context) const override { context.bind_sampler(m_binding, *m_sampler, m_array_index); }

private:
    RHIBindingId m_binding;
    RHISampler* m_sampler;
    uint32_t m_array_index;
};

class RHIDrawCommand final : public RHICommand
{
public:
    static constexpr RHICommandType TYPE = RHICommandType::Draw;

    RHIDrawCommand(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
        : RHICommand(TYPE)
        , m_vertex_count(vertex_count)
        , m_instance_count(instance_count)
        , m_first_vertex(first_vertex)
        , m_first_instance(first_instance)
    {
    }

    [[nodiscard]] uint32_t vertex_count() const { return m_vertex_count; }
    [[nodiscard]] uint32_t instance_count() const { return m_instance_count; }
    [[nodiscard]] uint32_t first_vertex() const { return m_first_vertex; }
    [[nodiscard]] uint32_t first_instance() const { return m_first_instance; }

    void execute(IRHICommandContext& context) const override { context.draw(m_vertex_count, m_instance_count, m_first_vertex, m_first_instance); }

private:
    uint32_t m_vertex_count;
    uint32_t m_instance_count;
    uint32_t m_first_vertex;
    uint32_t m_first_instance;
};

class RHIDrawIndexedCommand final : public RHICommand
{
public:
    static constexpr RHICommandType TYPE = RHICommandType::DrawIndexed;

    RHIDrawIndexedCommand(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t base_vertex, uint32_t first_instance)
        : RHICommand(TYPE)
        , m_index_count(index_count)
        , m_instance_count(instance_count)
        , m_first_index(first_index)
        , m_base_vertex(base_vertex)
        , m_first_instance(first_instance)
    {
    }

    [[nodiscard]] uint32_t index_count() const { return m_index_count; }
    [[nodiscard]] uint32_t instance_count() const { return m_instance_count; }
    [[nodiscard]] uint32_t first_index() const { return m_first_index; }
    [[nodiscard]] int32_t base_vertex() const { return m_base_vertex; }
    [[nodiscard]] uint32_t first_instance() const { return m_first_instance; }

    void execute(IRHICommandContext& context) const override
    {
        context.draw_indexed(m_index_count, m_instance_count, m_first_index, m_base_vertex, m_first_instance);
    }

private:
    uint32_t m_index_count;
    uint32_t m_instance_count;
    uint32_t m_first_index;
    int32_t m_base_vertex;
    uint32_t m_first_instance;
};

// `name` points into the recording list's arena.
class RHIPushDebugGroupCommand final : public RHICommand
{
public:
    static constexpr RHICommandType TYPE = RHICommandType::PushDebugGroup;

    explicit RHIPushDebugGroupCommand(const char* name)
        : RHICommand(TYPE)
        , m_name(name)
    {
    }

    [[nodiscard]] const char* name() const { return m_name; }

    void execute(IRHICommandContext& context) const override { context.push_debug_group(m_name); }

private:
    const char* m_name;
};

class RHIPopDebugGroupCommand final : public RHICommand
{
public:
    static constexpr RHICommandType TYPE = RHICommandType::PopDebugGroup;

    RHIPopDebugGroupCommand()
        : RHICommand(TYPE)
    {
    }

    void execute(IRHICommandContext& context) const override { context.pop_debug_group(); }
};

class RHIEndPassCommand final : public RHICommand
{
public:
    static constexpr RHICommandType TYPE = RHICommandType::EndPass;

    RHIEndPassCommand()
        : RHICommand(TYPE)
    {
    }

    void execute(IRHICommandContext& context) const override { context.end_pass(); }
};

// The command as T when its type matches, else null.
template<typename T>
[[nodiscard]] const T* command_cast(const RHICommand& command)
{
    return command.type() == T::TYPE ? static_cast<const T*>(&command) : nullptr;
}

} // namespace oryx
