#pragma once

#include "Oryx/Graphics/RHI/RHICommandContext.h"

namespace oryx
{

class RHICommandStream;

// Commands are arena-allocated by the stream and never destroyed individually, so they must stay trivially destructible and hold raw pointers.
class RHICommand
{
public:
    RHICommand(const RHICommand&) = delete;
    RHICommand& operator=(const RHICommand&) = delete;

    // Identifies the concrete command type; equal for every instance of one type.
    [[nodiscard]] const void* type_id() const { return m_type_id; }
    [[nodiscard]] const RHICommand* next() const { return m_next; }
    [[nodiscard]] virtual const char* command_name() const = 0;

    virtual void execute(IRHICommandContext& context) const = 0;

protected:
    explicit RHICommand(const void* type_id)
        : m_type_id(type_id)
    {
    }

    ~RHICommand() = default;

private:
    friend class RHICommandStream;

    const void* m_type_id;
    RHICommand* m_next = nullptr;
};

// CRTP: Derived supplies NAME and run(); the base derives the type id from Derived and bridges the virtual execute to run.
template<typename Derived>
class RHICommandT : public RHICommand
{
protected:
    RHICommandT()
        : RHICommand(static_type_id())
    {
    }

public:
    [[nodiscard]] static const void* static_type_id()
    {
        static const char tag = 0;
        return &tag;
    }

    [[nodiscard]] const char* command_name() const override final { return Derived::NAME; }

    void execute(IRHICommandContext& context) const override final { static_cast<const Derived&>(*this).run(context); }
};

class RHIBeginPassCommand final : public RHICommandT<RHIBeginPassCommand>
{
public:
    static constexpr const char* NAME = "BeginPass";

    explicit RHIBeginPassCommand(const RHIRenderPassDesc& pass)
        : m_pass(pass)
    {
    }

    [[nodiscard]] const RHIRenderPassDesc& pass() const { return m_pass; }

    void run(IRHICommandContext& context) const { context.begin_pass(m_pass); }

private:
    RHIRenderPassDesc m_pass;
};

class RHISetPipelineCommand final : public RHICommandT<RHISetPipelineCommand>
{
public:
    static constexpr const char* NAME = "SetPipeline";

    explicit RHISetPipelineCommand(RHIGraphicsPipeline& pipeline)
        : m_pipeline(&pipeline)
    {
    }

    [[nodiscard]] RHIGraphicsPipeline& pipeline() const { return *m_pipeline; }

    void run(IRHICommandContext& context) const { context.set_pipeline(*m_pipeline); }

private:
    RHIGraphicsPipeline* m_pipeline;
};

class RHISetViewportCommand final : public RHICommandT<RHISetViewportCommand>
{
public:
    static constexpr const char* NAME = "SetViewport";

    explicit RHISetViewportCommand(const RHIViewportState& viewport)
        : m_viewport(viewport)
    {
    }

    [[nodiscard]] const RHIViewportState& viewport() const { return m_viewport; }

    void run(IRHICommandContext& context) const { context.set_viewport(m_viewport); }

private:
    RHIViewportState m_viewport;
};

class RHISetScissorCommand final : public RHICommandT<RHISetScissorCommand>
{
public:
    static constexpr const char* NAME = "SetScissor";

    explicit RHISetScissorCommand(const RHIScissorRect& scissor)
        : m_scissor(scissor)
    {
    }

    [[nodiscard]] const RHIScissorRect& scissor() const { return m_scissor; }

    void run(IRHICommandContext& context) const { context.set_scissor(m_scissor); }

private:
    RHIScissorRect m_scissor;
};

class RHISetVertexBufferCommand final : public RHICommandT<RHISetVertexBufferCommand>
{
public:
    static constexpr const char* NAME = "SetVertexBuffer";

    RHISetVertexBufferCommand(uint32_t slot, RHIBuffer& buffer, uint32_t offset)
        : m_slot(slot)
        , m_buffer(&buffer)
        , m_offset(offset)
    {
    }

    [[nodiscard]] uint32_t slot() const { return m_slot; }
    [[nodiscard]] RHIBuffer& buffer() const { return *m_buffer; }
    [[nodiscard]] uint32_t offset() const { return m_offset; }

    void run(IRHICommandContext& context) const { context.set_vertex_buffer(m_slot, *m_buffer, m_offset); }

private:
    uint32_t m_slot;
    RHIBuffer* m_buffer;
    uint32_t m_offset;
};

class RHISetIndexBufferCommand final : public RHICommandT<RHISetIndexBufferCommand>
{
public:
    static constexpr const char* NAME = "SetIndexBuffer";

    RHISetIndexBufferCommand(RHIBuffer& buffer, uint32_t offset, bool index32)
        : m_buffer(&buffer)
        , m_offset(offset)
        , m_index32(index32)
    {
    }

    [[nodiscard]] RHIBuffer& buffer() const { return *m_buffer; }
    [[nodiscard]] uint32_t offset() const { return m_offset; }
    [[nodiscard]] bool index32() const { return m_index32; }

    void run(IRHICommandContext& context) const { context.set_index_buffer(*m_buffer, m_offset, m_index32); }

private:
    RHIBuffer* m_buffer;
    uint32_t m_offset;
    bool m_index32;
};

// `data` points into the recording list's arena.
class RHISetConstantsCommand final : public RHICommandT<RHISetConstantsCommand>
{
public:
    static constexpr const char* NAME = "SetConstants";

    RHISetConstantsCommand(RHIBindingId binding, const uint8_t* data, uint32_t size)
        : m_binding(binding)
        , m_data(data)
        , m_size(size)
    {
    }

    [[nodiscard]] RHIBindingId binding() const { return m_binding; }
    [[nodiscard]] const uint8_t* data() const { return m_data; }
    [[nodiscard]] uint32_t size() const { return m_size; }

    void run(IRHICommandContext& context) const { context.set_constants(m_binding, m_data, m_size); }

private:
    RHIBindingId m_binding;
    const uint8_t* m_data;
    uint32_t m_size;
};

class RHIBindBufferCommand final : public RHICommandT<RHIBindBufferCommand>
{
public:
    static constexpr const char* NAME = "BindBuffer";

    RHIBindBufferCommand(RHIBindingId binding, RHIBuffer& buffer, uint32_t offset, uint32_t size)
        : m_binding(binding)
        , m_buffer(&buffer)
        , m_offset(offset)
        , m_size(size)
    {
    }

    [[nodiscard]] RHIBindingId binding() const { return m_binding; }
    [[nodiscard]] RHIBuffer& buffer() const { return *m_buffer; }
    [[nodiscard]] uint32_t offset() const { return m_offset; }
    [[nodiscard]] uint32_t size() const { return m_size; }

    void run(IRHICommandContext& context) const { context.bind_buffer(m_binding, *m_buffer, m_offset, m_size); }

private:
    RHIBindingId m_binding;
    RHIBuffer* m_buffer;
    uint32_t m_offset;
    uint32_t m_size;
};

class RHIBindTextureCommand final : public RHICommandT<RHIBindTextureCommand>
{
public:
    static constexpr const char* NAME = "BindTexture";

    RHIBindTextureCommand(RHIBindingId binding, RHITexture& texture, uint32_t array_index)
        : m_binding(binding)
        , m_texture(&texture)
        , m_array_index(array_index)
    {
    }

    [[nodiscard]] RHIBindingId binding() const { return m_binding; }
    [[nodiscard]] RHITexture& texture() const { return *m_texture; }
    [[nodiscard]] uint32_t array_index() const { return m_array_index; }

    void run(IRHICommandContext& context) const { context.bind_texture(m_binding, *m_texture, m_array_index); }

private:
    RHIBindingId m_binding;
    RHITexture* m_texture;
    uint32_t m_array_index;
};

class RHIBindSamplerCommand final : public RHICommandT<RHIBindSamplerCommand>
{
public:
    static constexpr const char* NAME = "BindSampler";

    RHIBindSamplerCommand(RHIBindingId binding, RHISampler& sampler, uint32_t array_index)
        : m_binding(binding)
        , m_sampler(&sampler)
        , m_array_index(array_index)
    {
    }

    [[nodiscard]] RHIBindingId binding() const { return m_binding; }
    [[nodiscard]] RHISampler& sampler() const { return *m_sampler; }
    [[nodiscard]] uint32_t array_index() const { return m_array_index; }

    void run(IRHICommandContext& context) const { context.bind_sampler(m_binding, *m_sampler, m_array_index); }

private:
    RHIBindingId m_binding;
    RHISampler* m_sampler;
    uint32_t m_array_index;
};

class RHIDrawCommand final : public RHICommandT<RHIDrawCommand>
{
public:
    static constexpr const char* NAME = "Draw";

    RHIDrawCommand(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
        : m_vertex_count(vertex_count)
        , m_instance_count(instance_count)
        , m_first_vertex(first_vertex)
        , m_first_instance(first_instance)
    {
    }

    [[nodiscard]] uint32_t vertex_count() const { return m_vertex_count; }
    [[nodiscard]] uint32_t instance_count() const { return m_instance_count; }
    [[nodiscard]] uint32_t first_vertex() const { return m_first_vertex; }
    [[nodiscard]] uint32_t first_instance() const { return m_first_instance; }

    void run(IRHICommandContext& context) const { context.draw(m_vertex_count, m_instance_count, m_first_vertex, m_first_instance); }

private:
    uint32_t m_vertex_count;
    uint32_t m_instance_count;
    uint32_t m_first_vertex;
    uint32_t m_first_instance;
};

class RHIDrawIndexedCommand final : public RHICommandT<RHIDrawIndexedCommand>
{
public:
    static constexpr const char* NAME = "DrawIndexed";

    RHIDrawIndexedCommand(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t base_vertex, uint32_t first_instance)
        : m_index_count(index_count)
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

    void run(IRHICommandContext& context) const
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
class RHIPushDebugGroupCommand final : public RHICommandT<RHIPushDebugGroupCommand>
{
public:
    static constexpr const char* NAME = "PushDebugGroup";

    explicit RHIPushDebugGroupCommand(const char* name)
        : m_name(name)
    {
    }

    [[nodiscard]] const char* name() const { return m_name; }

    void run(IRHICommandContext& context) const { context.push_debug_group(m_name); }

private:
    const char* m_name;
};

class RHIPopDebugGroupCommand final : public RHICommandT<RHIPopDebugGroupCommand>
{
public:
    static constexpr const char* NAME = "PopDebugGroup";

    RHIPopDebugGroupCommand()
    {
    }

    void run(IRHICommandContext& context) const { context.pop_debug_group(); }
};

class RHIEndPassCommand final : public RHICommandT<RHIEndPassCommand>
{
public:
    static constexpr const char* NAME = "EndPass";

    RHIEndPassCommand()
    {
    }

    void run(IRHICommandContext& context) const { context.end_pass(); }
};

class RHICopyBufferCommand final : public RHICommandT<RHICopyBufferCommand>
{
public:
    static constexpr const char* NAME = "CopyBuffer";

    RHICopyBufferCommand(RHIBuffer& source, uint32_t source_offset, RHIBuffer& destination, uint32_t destination_offset, uint32_t size)
        : m_source(&source)
        , m_destination(&destination)
        , m_source_offset(source_offset)
        , m_destination_offset(destination_offset)
        , m_size(size)
    {
    }

    [[nodiscard]] RHIBuffer& source() const { return *m_source; }
    [[nodiscard]] RHIBuffer& destination() const { return *m_destination; }
    [[nodiscard]] uint32_t source_offset() const { return m_source_offset; }
    [[nodiscard]] uint32_t destination_offset() const { return m_destination_offset; }
    [[nodiscard]] uint32_t size() const { return m_size; }

    void run(IRHICommandContext& context) const { context.copy_buffer(*m_source, m_source_offset, *m_destination, m_destination_offset, m_size); }

private:
    RHIBuffer* m_source;
    RHIBuffer* m_destination;
    uint32_t m_source_offset;
    uint32_t m_destination_offset;
    uint32_t m_size;
};

// The command as T when its type matches, else null.
template<typename T>
[[nodiscard]] const T* command_cast(const RHICommand& command)
{
    return command.type_id() == T::static_type_id() ? static_cast<const T*>(&command) : nullptr;
}

} // namespace oryx
