#include "MoveTree.h"

#include "Oryx/Core/Error.h"

namespace oryx
{

namespace
{

struct EdgeKey
{
    uint32_t parent;
    PickKind kind;
    uint32_t value;

    bool operator==(const EdgeKey& other) const { return parent == other.parent && kind == other.kind && value == other.value; }
};

struct EdgeHash
{
    size_t operator()(const EdgeKey& key) const
    {
        uint64_t mixed = (static_cast<uint64_t>(key.parent) << 32) | key.value;
        mixed ^= static_cast<uint64_t>(key.kind) * 0x9E3779B97F4A7C15ull;
        mixed *= 0xBF58476D1CE4E5B9ull;
        return static_cast<size_t>(mixed ^ (mixed >> 31));
    }
};

} // namespace

void MoveTree::clear()
{
    m_nodes.assign(1, Node{});
    m_max_depth = 0;
}

void MoveTree::build(const std::vector<MoveCandidate>& candidates)
{
    clear();
    std::unordered_map<EdgeKey, uint32_t, EdgeHash> edges;
    for (const MoveCandidate& candidate : candidates)
    {
        if (candidate.picks.empty())
        {
            clear();
            throw Error("Action " + to_string(candidate.action) + " has no picks");
        }

        uint32_t current = k_root;
        for (const Pick& pick : candidate.picks)
        {
            if (pick.kind == PickKind::Confirm)
            {
                clear();
                throw Error("Action " + to_string(candidate.action) + " has a Confirm pick; only MoveBuilder offers Confirm");
            }

            auto found = edges.find({ current, pick.kind, pick.value });
            if (found == edges.end())
            {
                uint32_t created = static_cast<uint32_t>(m_nodes.size());
                Node node;
                node.pick = pick;
                m_nodes.push_back(std::move(node));
                Node& parent = m_nodes[current];
                if (parent.last_child == k_none)
                {
                    parent.first_child = created;
                }
                else
                {
                    m_nodes[parent.last_child].next_sibling = created;
                }
                parent.last_child = created;
                found = edges.emplace(EdgeKey{ current, pick.kind, pick.value }, created).first;
            }
            current = found->second;
        }

        if (m_nodes[current].action != INVALID_ACTION)
        {
            ActionId first = m_nodes[current].action;
            clear();
            throw Error("Actions " + to_string(first) + " and " + to_string(candidate.action) + " have the same picks");
        }
        m_nodes[current].action = candidate.action;
        m_max_depth = std::max(m_max_depth, candidate.picks.size());
    }
}

uint32_t MoveTree::child(uint32_t parent, const Pick& pick) const
{
    for (uint32_t index = m_nodes[parent].first_child; index != k_none; index = m_nodes[index].next_sibling)
    {
        if (m_nodes[index].pick == pick)
        {
            return index;
        }
    }
    return k_none;
}

} // namespace oryx
