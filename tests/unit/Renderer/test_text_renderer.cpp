#include "doctest.h"

#include "Oryx.h"
#include "NullRHI.h"
#include "Oryx/Renderer/RendererContext.h"
#include "FakeFontSource.h"

using namespace oryx;

namespace
{

const Colour RED = { 1.0f, 0.0f, 0.0f, 1.0f };

struct TextFixture
{
    UniquePtr<RendererContext> context = create_renderer_context({ RHIBackend::Null });
    std::vector<DrawItem> sink;
    Camera2D camera{ 800.0f, 600.0f };
    test::FakeFontSource* source = nullptr;
    Font font = test::make_fake_font(source);

    BatchRendererDesc desc() { return batch_renderer_desc(*context, sink); }

    std::vector<Vertex2DText> vertices(const DrawItem& item)
    {
        std::vector<Vertex2DText> result(item.vertex_count);
        std::memcpy(result.data(), item.vertex_buffers[0]->map() + item.vertex_offsets[0], item.vertex_count * sizeof(Vertex2DText));
        return result;
    }

    ~TextFixture() { context->rhi->wait_idle(); }
};

} // namespace

TEST_CASE("draw_text: one indexed quad per glyph in a single text draw")
{
    TextFixture f;
    BatchRenderer2D batcher(f.desc());
    TextStyle style;
    batcher.begin(f.camera);
    batcher.draw_text({ 0.0f, 0.0f }, "AB C", f.font, style);
    batcher.end();

    REQUIRE(f.sink.size() == 1);
    CHECK(f.sink[0].vertex_count == 12);
    CHECK(f.sink[0].index_count == 18);
    CHECK(f.sink[0].index_buffer == f.context->defaults.quad_indices);
    CHECK(f.sink[0].pipeline == f.context->pipeline_memo.get(*f.context->rhi, f.context->pipelines, f.context->shaders, f.context->back_buffer_format, pipeline_def(Primitive2D::Text), 1));
    CHECK(batcher.stats().primitives == 3);
    CHECK(batcher.stats().triangles == 6);
    CHECK(batcher.stats().texture_slots_used == 2);
    CHECK(f.sink[0].textures[1] == f.font.atlas(16.0f).texture(*f.context->rhi).texture());
    CHECK(f.sink[0].sampler == f.font.atlas(16.0f).texture(*f.context->rhi).sampler());
}

TEST_CASE("draw_text: glyph quads carry the atlas uvs, colour and positions")
{
    TextFixture f;
    BatchRenderer2D batcher(f.desc());
    TextStyle style;
    style.colour = RED;
    style.scale = 2.0f;
    batcher.begin(f.camera);
    batcher.draw_text({ 10.0f, 20.0f }, "A", f.font, style);
    batcher.end();

    const GlyphAtlasData& atlas = f.font.atlas(16.0f).data();
    const Glyph& glyph = atlas.glyph('A');
    const std::vector<Vertex2DText> v = f.vertices(f.sink[0]);
    REQUIRE(v.size() == 4);
    CHECK(v[0].base.position[0] == doctest::Approx(10.0f + glyph.bearing[0] * 2.0f));
    CHECK(v[0].base.position[1] == doctest::Approx(20.0f + glyph.bearing[1] * 2.0f));
    CHECK(v[2].base.position[0] == doctest::Approx(v[0].base.position[0] + glyph.size[0] * 2.0f));
    CHECK(v[2].base.position[1] == doctest::Approx(v[0].base.position[1] + glyph.size[1] * 2.0f));
    CHECK(v[0].uv[0] == doctest::Approx(glyph.uv_min[0]));
    CHECK(v[0].uv[1] == doctest::Approx(glyph.uv_max[1]));
    CHECK(v[2].uv[0] == doctest::Approx(glyph.uv_max[0]));
    CHECK(v[2].uv[1] == doctest::Approx(glyph.uv_min[1]));
    CHECK(v[0].base.colour[0] == 1.0f);
    CHECK(v[0].base.colour[1] == 0.0f);
    CHECK(v[0].px_range == 0.0f);
    CHECK(v[0].tex_index == v[3].tex_index);
}

TEST_CASE("draw_text: Centre and Right alignment shift the text left by half and all of its width")
{
    TextFixture f;
    TextStyle left;
    TextStyle centre;
    centre.align = TextAlign::Centre;
    TextStyle right;
    right.align = TextAlign::Right;
    const float width = f.font.measure("AB", left.pixel_height).width;

    auto first_x = [&](const TextStyle& style)
    {
        BatchRenderer2D batcher(f.desc());
        batcher.begin(f.camera);
        batcher.draw_text({ 100.0f, 0.0f }, "AB", f.font, style);
        batcher.end();
        return f.vertices(f.sink.back())[0].base.position[0];
    };

    float left_x = first_x(left);
    CHECK(first_x(centre) == doctest::Approx(left_x - 0.5f * width));
    CHECK(first_x(right) == doctest::Approx(left_x - width));
}

TEST_CASE("draw_text: newlines, kerning and UTF-8 offset the pen")
{
    TextFixture f;
    BatchRenderer2D batcher(f.desc());
    const GlyphAtlasData& atlas = f.font.atlas(16.0f).data();
    batcher.begin(f.camera);
    batcher.draw_text({ 0.0f, 0.0f }, "AV\nA", f.font);
    std::string accented;
    encode_utf8(0xE9, accented);
    batcher.draw_text({ 0.0f, 100.0f }, accented + accented, f.font);
    batcher.end();

    const Glyph& a = atlas.glyph('A');
    const Glyph& vee = atlas.glyph('V');
    const std::vector<Vertex2DText> v = f.vertices(f.sink[0]);
    REQUIRE(v.size() >= 12);
    CHECK(v[4].base.position[0] == doctest::Approx(a.advance + atlas.kerning('A', 'V') + vee.bearing[0]));
    CHECK(v[8].base.position[0] == doctest::Approx(a.bearing[0]));
    CHECK(v[8].base.position[1] == doctest::Approx(-atlas.line_height() + a.bearing[1]));
    CHECK(f.sink[0].vertex_count == 12 + 8);
}

TEST_CASE("draw_text: text and other streams flush on every switch")
{
    TextFixture f;
    BatchRenderer2D batcher(f.desc());
    batcher.begin(f.camera);
    batcher.draw_text({ 0.0f, 0.0f }, "AB", f.font);
    batcher.draw_text({ 0.0f, 20.0f }, "CD", f.font);
    batcher.draw_rect({ 0.0f, 0.0f }, { 4.0f, 4.0f }, RED);
    batcher.draw_text({ 0.0f, 40.0f }, "E", f.font);
    batcher.end();

    REQUIRE(f.sink.size() == 3);
    CHECK(f.sink[0].vertex_count == 16);
    CHECK(f.sink[1].vertex_count == 4);
    CHECK(f.sink[2].vertex_count == 4);
    CHECK(f.sink[0].pipeline != f.sink[1].pipeline);
    CHECK(batcher.stats().flushes[static_cast<uint32_t>(FlushReason::StreamChange)] == 2);
}

TEST_CASE("draw_text: spaces and empty text emit nothing")
{
    TextFixture f;
    BatchRenderer2D batcher(f.desc());
    batcher.begin(f.camera);
    batcher.draw_text({ 0.0f, 0.0f }, "", f.font);
    batcher.draw_text({ 0.0f, 0.0f }, "   \t\n", f.font);
    batcher.end();
    CHECK(f.sink.empty());
}

TEST_CASE("draw_text: a font that is not ready draws nothing, then draws once ready")
{
    TextFixture f;
    f.source->is_ready = false;
    BatchRenderer2D batcher(f.desc());
    batcher.begin(f.camera);
    batcher.draw_text({ 0.0f, 0.0f }, "AB", f.font);
    batcher.end();
    CHECK(f.sink.empty());

    f.source->is_ready = true;
    batcher.begin(f.camera);
    batcher.draw_text({ 0.0f, 0.0f }, "AB", f.font);
    batcher.end();
    CHECK(f.sink.size() == 1);
}

TEST_CASE("draw_text: outside a scene throws")
{
    TextFixture f;
    BatchRenderer2D batcher(f.desc());
    CHECK_THROWS_AS(batcher.draw_text({ 0.0f, 0.0f }, "A", f.font), Error);
    CHECK_THROWS_AS(batcher.draw_text({ 0.0f, 0.0f }, "", f.font), Error);
}

TEST_CASE("BatchStats: triangles follow each primitive and texture slots the busiest draw")
{
    TextFixture f;
    BatchRenderer2D batcher(f.desc());
    const Texture2D texture = Texture2D::create(*f.context->rhi, { .width = 1, .height = 1 });
    batcher.begin(f.camera);
    batcher.draw_rect({ 0.0f, 0.0f }, { 1.0f, 1.0f }, RED);
    batcher.draw_circle({ 0.0f, 0.0f }, 1.0f, RED);
    batcher.draw_line({ 0.0f, 0.0f }, { 1.0f, 1.0f }, RED);
    batcher.draw_triangle({ 0.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f }, RED);
    batcher.draw_sprite({ 0.0f, 0.0f }, { 1.0f, 1.0f }, texture);
    batcher.draw_sprite({ 0.0f, 0.0f }, { 1.0f, 1.0f }, f.font.atlas(16.0f).texture(*f.context->rhi));
    batcher.end();
    CHECK(batcher.stats().triangles == 2 + 2 + 0 + 1 + 2 + 2);
    CHECK(batcher.stats().texture_slots_used >= 1);

    batcher.recycle(0);
    CHECK(batcher.stats().triangles == 0);
    CHECK(batcher.stats().texture_slots_used == 0);
}

TEST_CASE("Renderer::default_font: draws text with no setup")
{
    TextFixture f;
    BatchRenderer2D batcher(f.desc());
    Font& font = *f.context->default_font;
    CHECK(font.ready());
    CHECK(font.measure("Hi", 16.0f).width > 8.0f);
    CHECK(font.measure("", 16.0f).width == 0.0f);

    batcher.begin(f.camera);
    batcher.draw_text({ 0.0f, 0.0f }, "Hi", font, {});
    batcher.draw_text({ 0.0f, 0.0f }, "", font, {});
    batcher.draw_text({ 0.0f, 0.0f }, "\xE4\xB8\xAD", font, {});
    batcher.end();
    REQUIRE(f.sink.size() == 1);
    CHECK(f.sink[0].vertex_count == 12);
}

TEST_CASE("DebugRenderer: text works out of the box through the renderer's default font")
{
    TextFixture f;
    CHECK(f.context->debug.font() == f.context->default_font.get());
    BatchRenderer2D batcher(f.desc());
    f.context->debug.text({ 4.0f, 4.0f }, "debug");
    batcher.begin(f.camera);
    f.context->debug.render(batcher);
    batcher.end();
    REQUIRE(f.sink.size() == 1);
    CHECK(f.sink[0].vertex_count == 20);
}
