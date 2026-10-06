#pragma once

#include "Oryx/Board/IBoardPresenter.h"

namespace oryx
{

struct MoveCandidate
{
    ActionId action = INVALID_ACTION;
    PickList picks;
};

// The candidates' pick sequences as a prefix tree, so a step costs the siblings it scans, not the number of legal moves. Private to MoveBuilder.
class MoveTree
{
public:
    static constexpr uint32_t k_root = 0;
    static constexpr uint32_t k_none = std::numeric_limits<uint32_t>::max();

    struct Node
    {
        Pick pick;
        ActionId action = INVALID_ACTION;
        uint32_t first_child = k_none;
        uint32_t last_child = k_none;
        uint32_t next_sibling = k_none;
    };

    MoveTree() { clear(); }

    // Throws Error for a candidate with no picks or a Confirm pick, or two candidates with equal picks. Children keep first-appearance order.
    void build(const std::vector<MoveCandidate>& candidates);
    void clear();

    [[nodiscard]] const Node& node(uint32_t index) const { return m_nodes[index]; }
    [[nodiscard]] uint32_t child(uint32_t parent, const Pick& pick) const;
    [[nodiscard]] bool is_move(uint32_t index) const { return m_nodes[index].action != INVALID_ACTION; }
    [[nodiscard]] bool has_children(uint32_t index) const { return m_nodes[index].first_child != k_none; }
    [[nodiscard]] size_t max_depth() const { return m_max_depth; }
    [[nodiscard]] bool empty() const { return !has_children(k_root); }

private:
    std::vector<Node> m_nodes;
    size_t m_max_depth = 0;
};

} // namespace oryx
