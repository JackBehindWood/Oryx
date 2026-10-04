#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Graphics/RHI/RHIFlags.h"

namespace oryx
{

template<typename T>
class Ref;

enum class RHIBackend : uint8_t
{
    Null,
    Metal,
    OpenGL,
    Vulkan,
    D3D12,
    WebGPU
};

enum class RHIBindingKind : uint8_t
{
    Constants,
    UniformBuffer,
    StorageBuffer,
    SampledTexture,
    StorageTexture,
    Sampler
};

enum class RHIShaderStageMask : uint8_t
{
    Vertex = BIT(0),
    Pixel = BIT(1)
};

template<>
inline constexpr bool rhi_flags_enum<RHIShaderStageMask> = true;

enum class RHIBufferUsage : uint8_t
{
    Vertex = BIT(0),
    Index = BIT(1),
    Uniform = BIT(2),
    Storage = BIT(3)
};

template<>
inline constexpr bool rhi_flags_enum<RHIBufferUsage> = true;

enum class RHIMemory : uint8_t
{
    CpuToGpu,
    GpuOnly
};

enum class RHIFormat : uint8_t
{
    Undefined,
    R8Unorm,
    RGBA8Unorm,
    BGRA8Unorm,
    RGBA16Float,
    Depth32Float
};

enum class RHIDataType : uint8_t
{
    Float,
    Int,
    UInt,
    Depth
};

enum class RHILoadAction : uint8_t
{
    Load,
    Clear,
    DontCare
};

enum class RHIStoreAction : uint8_t
{
    Store,
    DontCare
};

enum class RHIPipelineKind : uint8_t
{
    Graphics
};

enum class RHITextureUsage : uint8_t
{
    Sampled = BIT(0),
    RenderTarget = BIT(1),
    DepthStencil = BIT(2)
};

template<>
inline constexpr bool rhi_flags_enum<RHITextureUsage> = true;

enum class RHITextureDimension : uint8_t
{
    Tex2D,
    Tex2DArray,
    Cube,
    Tex3D,
    Tex2DMultisample
};

enum class RHIFilter : uint8_t
{
    Nearest,
    Linear
};

enum class RHIAddressMode : uint8_t
{
    Clamp,
    Repeat,
    Mirror
};

enum class RHIVertexFormat : uint8_t
{
    Float,
    Float2,
    Float3,
    Float4
};

enum class RHIVertexStep : uint8_t
{
    PerVertex,
    PerInstance
};

enum class RHITopology : uint8_t
{
    Points,
    Lines,
    LineStrip,
    Triangles,
    TriangleStrip
};

enum class RHICullMode : uint8_t
{
    None,
    Front,
    Back
};

enum class RHIFrontFace : uint8_t
{
    CounterClockwise,
    Clockwise
};

enum class RHIFillMode : uint8_t
{
    Solid,
    Wireframe
};

enum class RHIBlendFactor : uint8_t
{
    Zero,
    One,
    SrcColour,
    OneMinusSrcColour,
    DstColour,
    OneMinusDstColour,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha
};

enum class RHIBlendOp : uint8_t
{
    Add,
    Subtract,
    ReverseSubtract,
    Min,
    Max
};

enum class RHIColourWriteMask : uint8_t
{
    Red = BIT(0),
    Green = BIT(1),
    Blue = BIT(2),
    Alpha = BIT(3),
    All = 0xF
};

template<>
inline constexpr bool rhi_flags_enum<RHIColourWriteMask> = true;

enum class RHICompare : uint8_t
{
    Never,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always
};

enum class RHIStencilOp : uint8_t
{
    Keep,
    Zero,
    Replace,
    IncrementClamp,
    DecrementClamp,
    Invert,
    IncrementWrap,
    DecrementWrap
};

// Compute and tessellation stages are reserved; no backend creates them yet.
enum class RHIShaderStage : uint8_t
{
    Vertex,
    Pixel,
    Compute,
    TessControl,
    TessEval
};


class IRHI;
class IRHICommandContext;
class RHICommandList;
class RHIDeviceLease;

class RHIResource;
class RHIBuffer;
class RHITexture;
class RHISampler;
class RHIShader;
class RHIVertexShader;
class RHIPixelShader;
class RHIPipeline;
class RHIGraphicsPipeline;
class RHIRenderTarget;
class RHIViewport;

using RHIBufferPtr = Ref<RHIBuffer>;
using RHITexturePtr = Ref<RHITexture>;
using RHISamplerPtr = Ref<RHISampler>;
using RHIShaderPtr = Ref<RHIShader>;
using RHIVertexShaderPtr = Ref<RHIVertexShader>;
using RHIPixelShaderPtr = Ref<RHIPixelShader>;
using RHIGraphicsPipelinePtr = Ref<RHIGraphicsPipeline>;
using RHIRenderTargetPtr = Ref<RHIRenderTarget>;
using RHIViewportPtr = Ref<RHIViewport>;

} // namespace oryx
