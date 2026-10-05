#include "doctest.h"

#include "Oryx.h"

using namespace oryx;

TEST_CASE("grid_spaces numbers rows from the top and labels them chess-style from the bottom left")
{
    BoardView view;
    grid_spaces(3, 2, true, view);

    REQUIRE(view.spaces.size() == 6);
    CHECK(view.spaces[0].label == "a2");
    CHECK(view.spaces[2].label == "c2");
    CHECK(view.spaces[3].label == "a1");
    CHECK(view.spaces[3].position == Vec3f(0.0f, 0.0f, 0.0f));
    CHECK(view.spaces[2].position == Vec3f(2.0f, 1.0f, 0.0f));
    CHECK(view.spaces[3].tone != view.spaces[4].tone);
    CHECK(view.spaces[3].tone != view.spaces[0].tone);
    CHECK(view.column_labels == std::vector<std::string>{ "a", "b", "c" });
    CHECK(view.row_labels == std::vector<std::string>{ "1", "2" });

    grid_spaces(28, 1, false, view);
    CHECK(view.spaces[25].label == "z1");
    CHECK(view.spaces[26].label == "aa1");
    CHECK(view.spaces[27].tone == 0);
}

TEST_CASE("space_at finds the space under a point by its footprint and shape")
{
    BoardView view;
    grid_spaces(3, 3, false, view);
    CHECK(space_at(view, { 0.0f, 2.0f }) == 0);
    CHECK(space_at(view, { 2.4f, -0.4f }) == 8);
    CHECK(space_at(view, { 3.6f, 0.0f }) == kNoSpace);

    view.spaces[4].shape = SpaceShape::Circle;
    CHECK(space_at(view, { 1.0f, 1.0f }) == 4);
    CHECK(space_at(view, { 1.45f, 1.45f }) == kNoSpace);
}

TEST_CASE("changed_spaces lists the spaces whose pieces differ")
{
    BoardView before;
    grid_spaces(3, 1, false, before);
    BoardView after = before;
    before.pieces = { { 0, 0, 0 }, { 0, 1, 2 } };
    after.pieces = { { 0, 0, 1 }, { 0, 1, 2 } };

    CHECK(changed_spaces(before, after) == std::vector<SpaceId>{ 0, 1 });
    CHECK(changed_spaces(before, before).empty());

    after.pieces = { { 0, 0, 0 }, { 0, 0, 2 } };
    CHECK(changed_spaces(before, after) == std::vector<SpaceId>{ 2 });

    BoardView other;
    grid_spaces(2, 1, false, other);
    CHECK(changed_spaces(before, other).empty());
}

TEST_CASE("validate_view rejects views the front ends cannot use")
{
    BoardView view;
    grid_spaces(2, 2, false, view);
    CHECK_NOTHROW(validate_view(view, "test"));

    BoardView duplicate = view;
    duplicate.spaces[1].label = "a2";
    CHECK_THROWS_AS(validate_view(duplicate, "test"), Error);

    BoardView blank = view;
    blank.spaces[0].label.clear();
    CHECK_THROWS_AS(validate_view(blank, "test"), Error);

    BoardView spaced = view;
    spaced.spaces[0].label = "a 1";
    CHECK_THROWS_AS(validate_view(spaced, "test"), Error);

    BoardView stray = view;
    stray.pieces.push_back({ 0, 0, 4 });
    CHECK_THROWS_AS(validate_view(stray, "test"), Error);
}
