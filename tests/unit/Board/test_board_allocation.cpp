#include "doctest.h"

#include "unit/Board/BoardTestSupport.h"
#include "unit/MemoryTestSupport.h"

using namespace oryx;
using namespace oryx::test;

TEST_CASE("A board frame over an unchanged state allocates nothing once warm")
{
    BoardPresentation presentation(create_unique<Grid19Presenter>(), "dummy", k_all_seats);
    UniquePtr<IState> state = DummyGame(5).new_initial_state();
    BoardScene scene;

    for (uint32_t warm = 0; warm < 3; ++warm)
    {
        presentation.update(*state);
        presentation.build_scene(k_no_space, scene);
        presentation.build_scene(3, scene);
    }
    REQUIRE(presentation.builder().pick({ PickKind::Option, 1, "take1" }).action == 1);
    presentation.build_scene(k_no_space, scene);
    REQUIRE(scene.layout->space_count() == 361);

    MemoryStats before = all_allocations();
    for (uint32_t frame = 0; frame < 100; ++frame)
    {
        CHECK_FALSE(presentation.update(*state));
        presentation.build_scene(frame % 2 == 0 ? k_no_space : 3, scene);
    }
    MemoryStats delta = memory_delta(before, all_allocations());

    CHECK(delta.allocation_count == 0);
}
