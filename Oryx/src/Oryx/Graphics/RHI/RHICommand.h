#pragma once

#include "Oryx/Graphics/RHI/RHICommandContext.h"

namespace oryx
{

enum class RHICommandType : uint8_t
{
    BeginPass,
    SetPipeline,
    SetVertexBuffer,
    SetIndexBuffer,
    BindBuffer,
    BindTexture,
    BindSampler,
    Draw,
    DrawIndexed,
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

    RHIBeginPassCommand(RHIRenderTarget& target, const RHIClear& clear)
        : RHICommand(TYPE)
        , m_target(&target)
        , m_clear(clear)
    {
    }

    [[nodiscard]] RHIRenderTarget& target() const { return *m_target; }
    [[nodiscard]] const RHIClear& clear() const { return m_clear; }

    void execute(IRHICommandContext& context) const override { context.begin_pass(*m_target, m_clear); }

private:
    RHIRenderTarget* m_target;
    RHIClear m_clear;
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

class RHIBindBufferCommand final : public RHICommand
{
public:
    static constexpr RHICommandType TYPE = RHICommandType::BindBuffer;

    RHIBindBufferCommand(RHIBindingId binding, RHIBuffer& buffer)
        : RHICommand(TYPE)
        , m_binding(binding)
        , m_buffer(&buffer)
    {
    }

    [[nodiscard]] RHIBindingId binding() const { return m_binding; }
    [[nodiscard]] RHIBuffer& buffer() const { return *m_buffer; }

    void execute(IRHICommandContext& context) const override { context.bind_buffer(m_binding, *m_buffer); }

private:
    RHIBindingId m_binding;
    RHIBuffer* m_buffer;
};

class RHIBindTextureCommand final : public RHICommand
{
public:
    static constexpr RHICommandType TYPE = RHICommandType::BindTexture;

    RHIBindTextureCommand(RHIBindingId binding, RHITexture& texture)
        : RHICommand(TYPE)
        , m_binding(binding)
        , m_texture(&texture)
    {
    }

    [[nodiscard]] RHIBindingId binding() const { return m_binding; }
    [[nodiscard]] RHITexture& texture() const { return *m_texture; }

    void execute(IRHICommandContext& context) const override { context.bind_texture(m_binding, *m_texture); }

private:
    RHIBindingId m_binding;
    RHITexture* m_texture;
};

class RHIBindSamplerCommand final : public RHICommand
{
public:
    static constexpr RHICommandType TYPE = RHICommandType::BindSampler;

    RHIBindSamplerCommand(RHIBindingId binding, RHISampler& sampler)
        : RHICommand(TYPE)
        , m_binding(binding)
        , m_sampler(&sampler)
    {
    }

    [[nodiscard]] RHIBindingId binding() const { return m_binding; }
    [[nodiscard]] RHISampler& sampler() const { return *m_sampler; }

    void execute(IRHICommandContext& context) const override { context.bind_sampler(m_binding, *m_sampler); }

private:
    RHIBindingId m_binding;
    RHISampler* m_sampler;
};

class RHIDrawCommand final : public RHICommand
{
public:
    static constexpr RHICommandType TYPE = RHICommandType::Draw;

    RHIDrawCommand(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex)
        : RHICommand(TYPE)
        , m_vertex_count(vertex_count)
        , m_instance_count(instance_count)
        , m_first_vertex(first_vertex)
    {
    }

    [[nodiscard]] uint32_t vertex_count() const { return m_vertex_count; }
    [[nodiscard]] uint32_t instance_count() const { return m_instance_count; }
    [[nodiscard]] uint32_t first_vertex() const { return m_first_vertex; }

    void execute(IRHICommandContext& context) const override { context.draw(m_vertex_count, m_instance_count, m_first_vertex); }

private:
    uint32_t m_vertex_count;
    uint32_t m_instance_count;
    uint32_t m_first_vertex;
};

class RHIDrawIndexedCommand final : public RHICommand
{
public:
    static constexpr RHICommandType TYPE = RHICommandType::DrawIndexed;

    RHIDrawIndexedCommand(uint32_t index_count, uint32_t instance_count, uint32_t first_index)
        : RHICommand(TYPE)
        , m_index_count(index_count)
        , m_instance_count(instance_count)
        , m_first_index(first_index)
    {
    }

    [[nodiscard]] uint32_t index_count() const { return m_index_count; }
    [[nodiscard]] uint32_t instance_count() const { return m_instance_count; }
    [[nodiscard]] uint32_t first_index() const { return m_first_index; }

    void execute(IRHICommandContext& context) const override { context.draw_indexed(m_index_count, m_instance_count, m_first_index); }

private:
    uint32_t m_index_count;
    uint32_t m_instance_count;
    uint32_t m_first_index;
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
