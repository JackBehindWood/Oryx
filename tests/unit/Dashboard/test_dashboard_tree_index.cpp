#include "doctest.h"

#include "Oryx.h"
#include "unit/MemoryTestSupport.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

std::vector<SearchNode> sample()
{
    return {
        { -1, INVALID_ACTION, 100, 0.0 },
        { 0, 1, 30, 0.1 },
        { 0, 2, 60, 0.4 },
        { 0, 3, 10, -0.2 },
        { 2, 4, 40, 0.3 },
        { 2, 5, 20, 0.2 },
    };
}

std::vector<uint32_t> rows_of(const TreeIndex& index, std::vector<uint8_t> expanded)
{
    std::vector<uint32_t> rows;
    index.visible_rows(expanded.data(), rows);
    return rows;
}

} // namespace

TEST_CASE("tree index: children come in the chosen order and depth follows the parents")
{
    const std::vector<SearchNode> nodes = sample();
    TreeIndex index;
    index.build(nodes.data(), static_cast<uint32_t>(nodes.size()), TreeSort::Visits, true);
    CHECK(index.reachable_count() == 6);
    CHECK(index.max_depth() == 2);
    CHECK(index.first_root() == 0);
    CHECK(index.first_child(0) == 2);
    CHECK(index.next_sibling(2) == 1);
    CHECK(index.next_sibling(1) == 3);
    CHECK(index.depth(4) == 2);
    index.build(nodes.data(), static_cast<uint32_t>(nodes.size()), TreeSort::Order, true);
    CHECK(index.first_child(0) == 1);
    index.build(nodes.data(), static_cast<uint32_t>(nodes.size()), TreeSort::Value, false);
    CHECK(index.first_child(0) == 3);
}

TEST_CASE("tree index: the principal line follows the most visited child and shares are of the parent")
{
    const std::vector<SearchNode> nodes = sample();
    TreeIndex index;
    index.build(nodes.data(), static_cast<uint32_t>(nodes.size()), TreeSort::Order, true);
    CHECK(index.is_principal(0));
    CHECK(index.is_principal(2));
    CHECK(index.is_principal(4));
    CHECK_FALSE(index.is_principal(1));
    CHECK(index.share_of_parent(2) == doctest::Approx(0.6));
    CHECK(index.share_of_parent(0) == doctest::Approx(1.0));
}

TEST_CASE("tree index: visible rows honour expansion in preorder")
{
    const std::vector<SearchNode> nodes = sample();
    TreeIndex index;
    index.build(nodes.data(), static_cast<uint32_t>(nodes.size()), TreeSort::Visits, true);
    CHECK(rows_of(index, { 0, 0, 0, 0, 0, 0 }) == std::vector<uint32_t>{ 0 });
    CHECK(rows_of(index, { 1, 0, 0, 0, 0, 0 }) == std::vector<uint32_t>{ 0, 2, 1, 3 });
    CHECK(rows_of(index, { 1, 0, 1, 0, 0, 0 }) == std::vector<uint32_t>{ 0, 2, 4, 5, 1, 3 });
    CHECK(rows_of(index, { 1, 1, 1, 1, 1, 1 }).size() == 6);
}

TEST_CASE("tree index: a malformed list never throws and unreachable nodes are left out")
{
    std::vector<SearchNode> nodes = {
        { 7, 1, 5, 0.0 },
        { 1, 2, 5, 0.0 },
        { 3, 3, 5, 0.0 },
        { 2, 4, 5, 0.0 },
        { 0, 5, 5, 0.0 },
    };
    TreeIndex index;
    CHECK_NOTHROW(index.build(nodes.data(), static_cast<uint32_t>(nodes.size()), TreeSort::Visits, true));
    CHECK(index.first_root() == 0);
    CHECK(index.reachable(0));
    CHECK(index.reachable(4));
    CHECK(index.reachable(1));
    CHECK_FALSE(index.reachable(2));
    CHECK_FALSE(index.reachable(3));
    CHECK(index.reachable_count() == 3);
    std::vector<uint8_t> all(nodes.size(), 1);
    std::vector<uint32_t> rows;
    index.visible_rows(all.data(), rows);
    CHECK(rows == std::vector<uint32_t>{ 0, 4, 1 });
}

TEST_CASE("tree index: empty input and a very deep chain are safe")
{
    TreeIndex index;
    index.build(nullptr, 0, TreeSort::Visits, true);
    CHECK(index.node_count() == 0);
    CHECK(index.first_root() == k_no_tree_node);
    std::vector<uint32_t> rows;
    index.visible_rows(nullptr, rows);
    CHECK(rows.empty());

    std::vector<SearchNode> chain;
    for (int32_t i = 0; i < 200; ++i)
    {
        chain.push_back(SearchNode{ i - 1, 1, 10, 0.0 });
    }
    index.build(chain.data(), static_cast<uint32_t>(chain.size()), TreeSort::Visits, true);
    CHECK(index.max_depth() == k_max_tree_depth - 1);
    CHECK(index.reachable_count() == k_max_tree_depth);
}

TEST_CASE("tree index: rebuilding a same-sized tree allocates nothing")
{
    const std::vector<SearchNode> nodes = sample();
    TreeIndex index;
    std::vector<uint32_t> rows;
    std::vector<uint8_t> expanded(nodes.size(), 1);
    index.build(nodes.data(), static_cast<uint32_t>(nodes.size()), TreeSort::Visits, true);
    index.visible_rows(expanded.data(), rows);
    const MemoryStats before = all_allocations();
    index.build(nodes.data(), static_cast<uint32_t>(nodes.size()), TreeSort::Value, false);
    index.visible_rows(expanded.data(), rows);
    CHECK(memory_delta(before, all_allocations()).allocation_count == 0);
}
