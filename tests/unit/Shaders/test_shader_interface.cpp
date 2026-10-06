#include "doctest.h"

#include "ShaderTestSupport.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

using Tamper = std::function<void(ShaderReflection&)>;

VertexShaderPtr vertex_shader(IRHI& rhi, const Tamper& tamper = {})
{
    ShaderCompilerOutput output = compile_msl(ShaderStage::Vertex, "vs_main", SAMPLE_MSL);
    if (tamper)
    {
        tamper(output.reflection);
    }
    return make_ref<VertexShader>(output, 0, VertexShader::create_rhi_shader(rhi, output, "vs_main"));
}

PixelShaderPtr pixel_shader(IRHI& rhi, const Tamper& tamper = {})
{
    ShaderCompilerOutput output = compile_msl(ShaderStage::Pixel, "ps_main", SAMPLE_MSL);
    if (tamper)
    {
        tamper(output.reflection);
    }
    return make_ref<PixelShader>(output, 0, PixelShader::create_rhi_shader(rhi, output, "ps_main"));
}

GraphicsPipelineState sample_state(RHIVertexFormat colour = RHIVertexFormat::Float4)
{
    GraphicsPipelineState state;
    state.vertex_declaration = RHIVertexDeclarationBuilder()
                                   .stream(0, 36)
                                   .attribute(0, RHIVertexFormat::Float3, 0)
                                   .attribute(1, colour, 12)
                                   .attribute(2, RHIVertexFormat::Float2, 28)
                                   .build();
    return state;
}

ShaderBinding& parameter(ShaderReflection& reflection, const char* name)
{
    for (ShaderBinding& binding : reflection.parameters)
    {
        if (binding.name == name)
        {
            return binding;
        }
    }
    throw Error(std::string("no parameter ") + name);
}

void expect_interface_error(IRHI& rhi, const GraphicsShaderSet& set, const GraphicsPipelineState& state, const char* text)
{
    if (rhi.capabilities().validates_shader_interface)
    {
        CHECK_THROWS_WITH_AS((void)make_graphics_pipeline(rhi, set, state), doctest::Contains(text), Error);
    }
    else
    {
        CHECK_NOTHROW((void)make_graphics_pipeline(rhi, set, state));
    }
}

} // namespace

namespace oryx::test
{

void run_shader_interface_contract(IRHI& rhi)
{
    SUBCASE("a shader whose reflection matches its code builds a pipeline")
    {
        CHECK_NOTHROW((void)make_graphics_pipeline(rhi, { vertex_shader(rhi), pixel_shader(rhi) }, sample_state()));
    }

    SUBCASE("a constants size that differs from the code names the parameter")
    {
        const GraphicsShaderSet set{ vertex_shader(rhi, [](ShaderReflection& r) { parameter(r, "frame").size += 16; }), pixel_shader(rhi) };
        expect_interface_error(rhi, set, sample_state(), "shader parameter 'frame'");
    }

    SUBCASE("a slot that differs from the code names the parameter")
    {
        const GraphicsShaderSet set{ vertex_shader(rhi, [](ShaderReflection& r) { parameter(r, "frame").slot += 1; }), pixel_shader(rhi) };
        expect_interface_error(rhi, set, sample_state(), "shader parameter 'frame'");
    }

    SUBCASE("a texture array length that differs from the code names the parameter")
    {
        const GraphicsShaderSet set{ vertex_shader(rhi), pixel_shader(rhi, [](ShaderReflection& r) { parameter(r, "textures").array_count = 8; }) };
        expect_interface_error(rhi, set, sample_state(), "shader parameter 'textures'");
    }

    SUBCASE("a vertex input type that differs from the code is caught")
    {
        const GraphicsShaderSet set{
            vertex_shader(rhi,
                [](ShaderReflection& r) {
                    for (ShaderStageVariable& input : r.inputs)
                    {
                        if (input.location == 1) input.type.rows = 3;
                    }
                }),
            pixel_shader(rhi)
        };
        expect_interface_error(rhi, set, sample_state(RHIVertexFormat::Float3), "vertex attribute 1");
    }
}

} // namespace oryx::test

TEST_CASE("Shader interface contract: Null backend")
{
    UniquePtr<IRHI> rhi = create_rhi(RHIBackend::Null);
    REQUIRE(rhi);
    CHECK_FALSE(rhi->capabilities().validates_shader_interface);
    oryx::test::run_shader_interface_contract(*rhi);
}

#ifdef OX_PLATFORM_MACOS
TEST_CASE("Shader interface contract: Metal backend")
{
    UniquePtr<IRHI> rhi;
    try
    {
        rhi = create_rhi(RHIBackend::Metal);
    }
    catch (const Error& error)
    {
        MESSAGE("Metal unavailable, skipping: ", error.what());
        return;
    }
#ifndef OX_DIST
    CHECK(rhi->capabilities().validates_shader_interface);
#endif
    oryx::test::run_shader_interface_contract(*rhi);
}
#endif

#ifdef OX_PLATFORM_MACOS
TEST_CASE("Shader interface: every built-in pipeline matches Metal's reflection, including the unattributed texture arrays")
{
    UniquePtr<IRHI> rhi;
    try
    {
        rhi = create_rhi(RHIBackend::Metal);
    }
    catch (const Error& error)
    {
        MESSAGE("Metal unavailable, skipping: ", error.what());
        return;
    }
    if (!rhi->capabilities().validates_shader_interface)
    {
        return;
    }

    ShaderCache cache;
    ShaderLibrary library;
    try
    {
        library.compile_all(*rhi, cache);
    }
    catch (const Error& error)
    {
        MESSAGE("shaders unavailable, skipping: ", error.what());
        return;
    }

    const auto build = [&](const GraphicsShaderSet& set, const RHIVertexDeclaration& declaration) {
        GraphicsPipelineState state;
        state.vertex_declaration = declaration;
        state.colour_formats[0] = RHIFormat::RGBA8Unorm;
        state.blend[0] = rhi_blend_alpha();
        return make_graphics_pipeline(*rhi, set, state);
    };
    CHECK_NOTHROW((void)build({ library.get<SolidVS>(), library.get<SolidPS>() }, vertex_declaration<Vertex2DLine>()));
    CHECK_NOTHROW((void)build({ library.get<CircleVS>(), library.get<CirclePS>() }, vertex_declaration<Vertex2DCircle>()));
    CHECK_NOTHROW((void)build({ library.get<TextVS>(), library.get<TextPS>() }, vertex_declaration<Vertex2DText>()));
    for (uint32_t permutation = 0; permutation < 2; ++permutation)
    {
        CAPTURE(permutation);
        CHECK_NOTHROW((void)build({ library.get<QuadVS>(), library.get<QuadPS>(permutation) }, vertex_declaration<Vertex2DQuad>()));
    }
}
#endif
