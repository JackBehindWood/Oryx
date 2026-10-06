#include "doctest.h"

#include "unit/Board/BoardTestSupport.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

class SpatialLayout : public BoardLayout
{
public:
    SpatialLayout()
        : BoardLayout(BoardDimension::Spatial)
    {
    }
};

} // namespace

TEST_CASE("make_grid_layout numbers rows from the top and labels them chess-style from the bottom left")
{
    SharedPtr<const BoardLayout2D> layout = make_grid_layout(3, 2, true);

    REQUIRE(layout->space_count() == 6);
    CHECK(layout->label(0) == "a2");
    CHECK(layout->label(2) == "c2");
    CHECK(layout->label(3) == "a1");
    CHECK(layout->position(3) == Vec2f(0.0f, 0.0f));
    CHECK(layout->position(2) == Vec2f(2.0f, 1.0f));
    CHECK(layout->tone(3) != layout->tone(4));
    CHECK(layout->tone(3) != layout->tone(0));
    CHECK(layout->column_labels() == std::vector<std::string>{ "a", "b", "c" });
    CHECK(layout->row_labels() == std::vector<std::string>{ "1", "2" });
    CHECK(layout->columns() == std::vector<float>{ 0.0f, 1.0f, 2.0f });
    CHECK(layout->rows() == std::vector<float>{ 1.0f, 0.0f });
    CHECK(layout->column_index(2) == 2);
    CHECK(layout->row_index(2) == 0);
    CHECK(layout->min() == Vec2f(-0.5f, -0.5f));
    CHECK(layout->max() == Vec2f(2.5f, 1.5f));
    CHECK(layout->text_grid());
    CHECK(layout->dimension() == BoardDimension::Planar);

    SharedPtr<const BoardLayout2D> wide = make_grid_layout(28, 1, false);
    CHECK(wide->label(25) == "z1");
    CHECK(wide->label(26) == "aa1");
    CHECK(wide->tone(27) == 0);
}

TEST_CASE("shared_grid_layout returns the same layout for the same grid")
{
    CHECK(shared_grid_layout(4, 4, true) == shared_grid_layout(4, 4, true));
    CHECK(shared_grid_layout(4, 4, true) != shared_grid_layout(4, 4, false));
    CHECK(shared_grid_layout(4, 4, true) != shared_grid_layout(5, 4, true));
}

TEST_CASE("space_at finds the space under a point by its footprint and shape")
{
    CHECK(space_at(*make_grid_layout(3, 3, false), { 0.0f, 2.0f }) == 0);
    CHECK(space_at(*make_grid_layout(3, 3, false), { 2.4f, -0.4f }) == 8);
    CHECK(space_at(*make_grid_layout(3, 3, false), { 3.6f, 0.0f }) == k_no_space);

    SharedPtr<const BoardLayout2D> ring = make_ring_layout();
    CHECK(space_at(*ring, ring->position(4)) == 4);
    CHECK(space_at(*ring, { ring->position(4)[0] + 0.39f, ring->position(4)[1] + 0.39f }) == k_no_space);
    CHECK(space_at(*ring, { 0.0f, 0.0f }) == k_no_space);
}

TEST_CASE("BoardLayout2D::finish rejects layouts the front ends cannot use")
{
    auto build = [](const std::string& first, const std::string& second, const Vec2f& size)
    {
        BoardLayout2D layout;
        layout.add_space(first, { 0.0f, 0.0f }, size, SpaceShape::Square);
        layout.add_space(second, { 1.0f, 0.0f }, { 1.0f, 1.0f }, SpaceShape::Square);
        return layout.finish();
    };

    CHECK_NOTHROW(build("a", "b", { 1.0f, 1.0f }));
    CHECK_THROWS_AS(build("a", "a", { 1.0f, 1.0f }), Error);
    CHECK_THROWS_AS(build("", "b", { 1.0f, 1.0f }), Error);
    CHECK_THROWS_AS(build("a 1", "b", { 1.0f, 1.0f }), Error);
    CHECK_THROWS_AS(build("a", "b", { 0.0f, 1.0f }), Error);
    CHECK_THROWS_AS(build("a", "b", { std::numeric_limits<float>::infinity(), 1.0f }), Error);
}

TEST_CASE("text_grid is false when two spaces share a column and row")
{
    BoardLayout2D builder;
    builder.add_space("low", { 0.0f, 0.0f }, { 1.0f, 1.0f }, SpaceShape::Square);
    builder.add_space("high", { 0.0f, 0.0001f }, { 1.0f, 1.0f }, SpaceShape::Square);
    CHECK_FALSE(builder.finish()->text_grid());
    CHECK(make_ring_layout()->text_grid());
}

TEST_CASE("layout_as checks the layout kind and names the game")
{
    SharedPtr<const BoardLayout> layout = make_grid_layout(2, 2, false);
    CHECK(&layout_as<BoardLayout2D>(layout, "test") == layout.get());
    CHECK_THROWS_AS(layout_as<BoardLayout2D>(SharedPtr<const BoardLayout>(), "test"), Error);
    SharedPtr<const BoardLayout> spatial = create_shared<SpatialLayout>();
    CHECK_THROWS_AS(layout_as<BoardLayout2D>(spatial, "test"), Error);
}

TEST_CASE("changed_spaces lists the spaces whose pieces differ")
{
    BoardView before;
    before.layout = make_grid_layout(3, 1, false);
    BoardView after = before;
    before.pieces = { { 0, 0, 0 }, { 0, 1, 2 } };
    after.pieces = { { 0, 0, 1 }, { 0, 1, 2 } };

    CHECK(changed_spaces(before, after) == std::vector<SpaceId>{ 0, 1 });
    CHECK(changed_spaces(before, before).empty());

    after.pieces = { { 0, 0, 0 }, { 0, 0, 2 } };
    CHECK(changed_spaces(before, after) == std::vector<SpaceId>{ 2 });

    BoardView other;
    other.layout = make_grid_layout(2, 1, false);
    CHECK(changed_spaces(before, other).empty());
}

TEST_CASE("validate_view rejects views the front ends cannot use")
{
    BoardView view;
    view.layout = make_grid_layout(2, 2, false);
    CHECK_NOTHROW(validate_view(view, "test"));

    BoardView stray = view;
    stray.pieces.push_back({ 0, 0, 4 });
    CHECK_THROWS_AS(validate_view(stray, "test"), Error);

    CHECK_THROWS_AS(validate_view(BoardView{}, "test"), Error);
}
