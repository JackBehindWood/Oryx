#pragma once

#include "Oryx/Strategy/Observability/Decision.h"

namespace oryx
{

enum class TreeSort : uint8_t
{
    Order,
    Visits,
    Value
};

constexpr uint32_t k_no_tree_node = std::numeric_limits<uint32_t>::max();
constexpr uint32_t k_max_tree_depth = 64;

// Children and depth of a flat SearchNode list; never throws, so a malformed tree from any strategy cannot abort a Match.
// A node whose parent is out of range, itself or missing becomes a root; nodes in a parent cycle are unreachable and left out.
class TreeIndex
{
public:
    void build(const SearchNode* nodes, uint32_t count, TreeSort sort, bool descending);

    [[nodiscard]] uint32_t node_count() const { return m_count; }
    [[nodiscard]] uint32_t reachable_count() const { return m_reachable; }
    [[nodiscard]] uint32_t max_depth() const { return m_max_depth; }
    [[nodiscard]] uint32_t first_root() const { return m_count == 0 ? k_no_tree_node : m_first_child[m_count]; }
    [[nodiscard]] uint32_t first_child(uint32_t node) const { return m_first_child[node]; }
    [[nodiscard]] uint32_t next_sibling(uint32_t node) const { return m_next_sibling[node]; }
    [[nodiscard]] uint32_t parent(uint32_t node) const { return m_parent[node]; }
    [[nodiscard]] uint32_t depth(uint32_t node) const { return m_depth[node]; }
    [[nodiscard]] bool is_principal(uint32_t node) const { return m_principal[node] != 0; }
    [[nodiscard]] bool reachable(uint32_t node) const { return m_depth[node] != k_unreached; }
    [[nodiscard]] bool has_children(uint32_t node) const { return m_first_child[node] != k_no_tree_node; }
    [[nodiscard]] const SearchNode& node(uint32_t index) const { return m_nodes[index]; }

    // Visits of a node over its parent's (roots: over the sum of the roots); NaN when that total is zero.
    [[nodiscard]] double share_of_parent(uint32_t node) const;

    // Preorder of the nodes whose every ancestor is expanded (one flag per node); `rows` keeps its capacity.
    void visible_rows(const uint8_t* expanded, std::vector<uint32_t>& rows) const;

private:
    static constexpr uint32_t k_unreached = std::numeric_limits<uint32_t>::max();

    const SearchNode* m_nodes = nullptr;
    uint32_t m_count = 0;
    uint32_t m_reachable = 0;
    uint32_t m_max_depth = 0;
    std::vector<uint32_t> m_parent;
    std::vector<uint32_t> m_first_child;
    std::vector<uint32_t> m_next_sibling;
    std::vector<uint32_t> m_depth;
    std::vector<uint32_t> m_order;
    std::vector<uint32_t> m_stack;
    std::vector<uint32_t> m_best_child;
    std::vector<uint8_t> m_principal;
    std::vector<double> m_group_visits;
};

} // namespace oryx
