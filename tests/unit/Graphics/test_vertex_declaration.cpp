#include "doctest.h"

#include "Oryx.h"

using namespace oryx;

TEST_CASE("RHIVertexDeclaration: a builder produces an immutable multi-stream description")
{
    const RHIVertexDeclaration declaration = RHIVertexDeclarationBuilder()
                                                 .stream(0, 12)
                                                 .stream(1, 16, RHIVertexStep::PerInstance, 2)
                                                 .attribute(0, RHIVertexFormat::Float3, 0, 0)
                                                 .attribute(1, RHIVertexFormat::Float4, 0, 1)
                                                 .build();
    CHECK(declaration.stream_count() == 2);
    CHECK(declaration.stride(0) == 12);
    CHECK(declaration.stream(1).step_function == RHIVertexStep::PerInstance);
    CHECK(declaration.stream(1).step_rate == 2);
    CHECK(declaration.attributes().size() == 2);

    const RHIVertexInput input = declaration.input();
    CHECK(input.attribute_count == 2);
    CHECK(input.streams[1].stride == 16);
    CHECK(input.streams[2].stride == 0);
    CHECK_THROWS_AS(declaration.stream(RHI_MAX_VERTEX_SLOTS), Error);
}

TEST_CASE("RHIVertexDeclaration: the builder rejects invalid descriptions")
{
    CHECK_THROWS_AS(RHIVertexDeclarationBuilder().stream(RHI_MAX_VERTEX_SLOTS, 4), Error);
    CHECK_THROWS_AS(RHIVertexDeclarationBuilder().stream(0, 0), Error);
    CHECK_THROWS_AS(RHIVertexDeclarationBuilder().stream(0, 4, RHIVertexStep::PerVertex, 0), Error);
    CHECK_THROWS_AS(RHIVertexDeclarationBuilder().attribute(0, RHIVertexFormat::Float, 0, RHI_MAX_VERTEX_SLOTS), Error);
    CHECK_THROWS_AS(RHIVertexDeclarationBuilder().attribute(0, RHIVertexFormat::Float, 0).build(), Error);
    CHECK_THROWS_AS(RHIVertexDeclarationBuilder().stream(0, 8).attribute(0, RHIVertexFormat::Float3, 0).build(), Error);
    CHECK_THROWS_AS(RHIVertexDeclarationBuilder().stream(0, 16).attribute(0, RHIVertexFormat::Float, 0).attribute(0, RHIVertexFormat::Float, 4).build(), Error);
}

TEST_CASE("RHIVertexDeclaration: hash and equality follow every field")
{
    const auto base = [] { return RHIVertexDeclarationBuilder().stream(0, 28).attribute(0, RHIVertexFormat::Float3, 0).attribute(1, RHIVertexFormat::Float4, 12); };
    const RHIVertexDeclaration reference = base().build();
    CHECK(reference == base().build());
    CHECK(reference.hash() == base().build().hash());
    CHECK(reference.hash() != RHIVertexDeclaration().hash());
    CHECK(RHIVertexDeclaration() == RHIVertexDeclarationBuilder().build());

    std::vector<RHIVertexDeclaration> others;
    others.push_back(RHIVertexDeclarationBuilder().stream(0, 32).attribute(0, RHIVertexFormat::Float3, 0).attribute(1, RHIVertexFormat::Float4, 12).build());
    others.push_back(RHIVertexDeclarationBuilder().stream(0, 28, RHIVertexStep::PerInstance).attribute(0, RHIVertexFormat::Float3, 0).attribute(1, RHIVertexFormat::Float4, 12).build());
    others.push_back(RHIVertexDeclarationBuilder().stream(0, 28, RHIVertexStep::PerVertex, 2).attribute(0, RHIVertexFormat::Float3, 0).attribute(1, RHIVertexFormat::Float4, 12).build());
    others.push_back(RHIVertexDeclarationBuilder().stream(0, 28).attribute(0, RHIVertexFormat::Float3, 0).build());
    others.push_back(RHIVertexDeclarationBuilder().stream(0, 28).attribute(0, RHIVertexFormat::Float3, 0).attribute(2, RHIVertexFormat::Float4, 12).build());
    others.push_back(RHIVertexDeclarationBuilder().stream(0, 28).attribute(0, RHIVertexFormat::Float3, 0).attribute(1, RHIVertexFormat::Half4, 12).build());
    others.push_back(RHIVertexDeclarationBuilder().stream(0, 28).attribute(0, RHIVertexFormat::Float3, 4).attribute(1, RHIVertexFormat::Float4, 12).build());
    for (const RHIVertexDeclaration& other : others)
    {
        CHECK(other.hash() != reference.hash());
        CHECK_FALSE(other == reference);
    }
}

TEST_CASE("RHIVertexFormat: byte sizes, component counts and shader compatibility")
{
    CHECK(rhi_vertex_format_bytes(RHIVertexFormat::Half2) == 4);
    CHECK(rhi_vertex_format_bytes(RHIVertexFormat::Half4) == 8);
    CHECK(rhi_vertex_format_bytes(RHIVertexFormat::UByte4Norm) == 4);
    CHECK(rhi_vertex_format_bytes(RHIVertexFormat::UInt3) == 12);
    CHECK(rhi_vertex_format_bytes(RHIVertexFormat::Int4) == 16);
    CHECK(rhi_vertex_format_components(RHIVertexFormat::UByte4Norm) == 4);
    CHECK(rhi_vertex_format_components(RHIVertexFormat::Half2) == 2);
    CHECK(rhi_vertex_format_is_float(RHIVertexFormat::Half4));
    CHECK_FALSE(rhi_vertex_format_is_float(RHIVertexFormat::UInt));

    const ShaderDataType float4 = { ShaderScalar::Float, 4, 1 };
    CHECK(rhi_vertex_format_feeds(RHIVertexFormat::Float4, float4));
    CHECK(rhi_vertex_format_feeds(RHIVertexFormat::Half4, float4));
    CHECK(rhi_vertex_format_feeds(RHIVertexFormat::UByte4Norm, float4));
    CHECK_FALSE(rhi_vertex_format_feeds(RHIVertexFormat::UInt4, float4));
    CHECK_FALSE(rhi_vertex_format_feeds(RHIVertexFormat::Float3, float4));
}
