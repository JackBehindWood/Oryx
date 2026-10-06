#include "doctest.h"

#include "Oryx.h"

using namespace oryx;

TEST_CASE("vertex_declaration: built-in 2D vertex formats match their structs")
{
    const RHIVertexDeclaration& line = vertex_declaration<Vertex2DLine>();
    CHECK(line.stride() == sizeof(Vertex2DLine));
    CHECK(line.attributes().size() == 2);
    CHECK(line.attributes()[1].offset == offsetof(Vertex2DBase, colour));

    const RHIVertexDeclaration& quad = vertex_declaration<Vertex2DQuad>();
    CHECK(quad.stride() == sizeof(Vertex2DQuad));
    REQUIRE(quad.attributes().size() == 4);
    CHECK(quad.attributes()[2].offset == offsetof(Vertex2DQuad, uv));
    CHECK(quad.attributes()[3].offset == offsetof(Vertex2DQuad, tex_index));

    const RHIVertexDeclaration& circle = vertex_declaration<Vertex2DCircle>();
    CHECK(circle.stride() == sizeof(Vertex2DCircle));
    REQUIRE(circle.attributes().size() == 5);
    CHECK(circle.attributes()[4].offset == offsetof(Vertex2DCircle, fade));

    const RHIVertexDeclaration& text = vertex_declaration<Vertex2DText>();
    CHECK(text.stride() == sizeof(Vertex2DText));
    REQUIRE(text.attributes().size() == 5);
    CHECK(text.attributes()[3].offset == offsetof(Vertex2DText, tex_index));
    CHECK(text.attributes()[4].offset == offsetof(Vertex2DText, px_range));
    CHECK(text.hash() != quad.hash());

    CHECK(line.hash() != quad.hash());
    CHECK(quad.hash() != circle.hash());
    CHECK(&vertex_declaration<Vertex2DQuad>() == &quad);
}

TEST_CASE("primitive_traits: agree with the vertex formats and effects")
{
    CHECK(primitive_traits(Primitive2D::Quad).vertex_size == vertex_declaration<Vertex2DQuad>().stride());
    CHECK(primitive_traits(Primitive2D::Circle).vertex_size == vertex_declaration<Vertex2DCircle>().stride());
    CHECK(primitive_traits(Primitive2D::Line).vertex_size == vertex_declaration<Vertex2DLine>().stride());
    CHECK(primitive_traits(Primitive2D::Triangle).vertex_size == vertex_declaration<Vertex2DLine>().stride());

    CHECK(primitive_traits(Primitive2D::Text).vertex_size == vertex_declaration<Vertex2DText>().stride());
    CHECK(primitive_traits(Primitive2D::Text).textured);
    CHECK_FALSE(primitive_traits(Primitive2D::Circle).textured);
    CHECK(primitive_traits(Primitive2D::Text).indices_per_primitive == 6);

    CHECK(primitive_traits(Primitive2D::Quad).indexed);
    CHECK(primitive_traits(Primitive2D::Quad).indices_per_primitive == 6);
    CHECK_FALSE(primitive_traits(Primitive2D::Line).indexed);
    CHECK(primitive_traits(Primitive2D::Line).vertices_per_primitive == 2);
    CHECK(primitive_traits(Primitive2D::Triangle).vertices_per_primitive == 3);

    for (uint32_t i = 0; i < PRIMITIVE_2D_COUNT; ++i)
    {
        const PrimitiveTraits traits = primitive_traits(static_cast<Primitive2D>(i));
        CHECK_FALSE(traits.instanced);
        CHECK(traits.vertices_per_primitive > 0);
        CHECK(traits.indexed == (traits.indices_per_primitive > 0));
    }
    CHECK_THROWS_AS(primitive_traits(static_cast<Primitive2D>(PRIMITIVE_2D_COUNT)), Error);
}
