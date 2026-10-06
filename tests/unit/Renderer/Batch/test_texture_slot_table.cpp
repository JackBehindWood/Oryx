#include "doctest.h"

#include "Oryx.h"
#include "NullRHI.h"

using namespace oryx;

TEST_CASE("TextureSlotTable: slot 0 is white, textures are reused and the table fills")
{
    NullRHI rhi;
    RHITexturePtr white = rhi.create_texture({ .width = 1, .height = 1 });
    RHITexturePtr a = rhi.create_texture({ .width = 1, .height = 1 });
    RHITexturePtr b = rhi.create_texture({ .width = 1, .height = 1 });
    RHITexturePtr c = rhi.create_texture({ .width = 1, .height = 1 });

    TextureSlotTable table(white, 3);
    CHECK(table.capacity() == 3);
    CHECK(table.count() == 1);
    CHECK(table.acquire({}) == 0);
    CHECK(table.acquire(white) == 0);
    CHECK(table.acquire(a) == 1);
    CHECK(table.acquire(a) == 1);
    CHECK(table.acquire(b) == 2);
    CHECK(table.acquire(c) == TextureSlotTable::FULL);
    CHECK(table.acquire(b) == 2);
    CHECK(table.texture(0) == white);

    table.reset();
    CHECK(table.count() == 1);
    CHECK(table.acquire(c) == 1);
    CHECK(table.texture(0) == white);
}

TEST_CASE("TextureSlotTable: rejects a missing white texture or no slots")
{
    NullRHI rhi;
    RHITexturePtr white = rhi.create_texture({ .width = 1, .height = 1 });
    CHECK_THROWS_AS(TextureSlotTable({}, 4), Error);
    CHECK_THROWS_AS(TextureSlotTable(white, 0), Error);
}
