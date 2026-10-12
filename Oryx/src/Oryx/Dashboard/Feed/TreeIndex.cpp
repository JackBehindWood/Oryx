#include "oxpch.h"
#include "Oryx/Dashboard/Feed/TreeIndex.h"

namespace oryx
{

namespace
{

bool better_child(const SearchNode& a, uint32_t a_index, const SearchNode& b, uint32_t b_index)
{
    if (a.visits != b.visits)
    {
        return a.visits > b.visits;
    }
    if (a.value != b.value)
    {
        return a.value > b.value;
    }
    return a_index < b_index;
}

} // namespace

void TreeIndex::build(const SearchNode* nodes, uint32_t count, TreeSort sort, bool descending)
{
    m_nodes = nodes;
    m_count = nodes == nullptr ? 0 : count;
    m_reachable = 0;
    m_max_depth = 0;
    const uint32_t groups = m_count + 1;
    m_parent.assign(m_count, k_no_tree_node);
    m_first_child.assign(groups, k_no_tree_node);
    m_next_sibling.assign(m_count, k_no_tree_node);
    m_depth.assign(m_count, k_unreached);
    m_best_child.assign(groups, k_no_tree_node);
    m_principal.assign(m_count, 0);
    m_group_visits.assign(groups, 0.0);
    m_order.resize(m_count);
    for (uint32_t i = 0; i < m_count; ++i)
    {
        const int32_t parent = nodes[i].parent;
        m_parent[i] = parent >= 0 && static_cast<uint32_t>(parent) < m_count && static_cast<uint32_t>(parent) != i ? static_cast<uint32_t>(parent) : m_count;
        m_order[i] = i;
        m_group_visits[m_parent[i]] += static_cast<double>(nodes[i].visits);
        uint32_t& best = m_best_child[m_parent[i]];
        if (best == k_no_tree_node || better_child(nodes[i], i, nodes[best], best))
        {
            best = i;
        }
    }

    std::sort(m_order.begin(), m_order.end(),
        [&](uint32_t a, uint32_t b)
        {
            if (m_parent[a] != m_parent[b])
            {
                return m_parent[a] < m_parent[b];
            }
            if (sort != TreeSort::Order)
            {
                const bool by_visits = sort == TreeSort::Visits;
                const double key_a = by_visits ? static_cast<double>(nodes[a].visits) : nodes[a].value;
                const double key_b = by_visits ? static_cast<double>(nodes[b].visits) : nodes[b].value;
                if (key_a != key_b)
                {
                    return descending ? key_a > key_b : key_a < key_b;
                }
            }
            return a < b;
        });
    for (uint32_t i = 0; i < m_count; ++i)
    {
        const uint32_t node = m_order[i];
        const uint32_t group = m_parent[node];
        if (m_first_child[group] == k_no_tree_node)
        {
            m_first_child[group] = node;
        }
        if (i + 1 < m_count && m_parent[m_order[i + 1]] == group)
        {
            m_next_sibling[node] = m_order[i + 1];
        }
    }

    m_stack.clear();
    for (uint32_t root = m_first_child[m_count]; root != k_no_tree_node; root = m_next_sibling[root])
    {
        m_depth[root] = 0;
        m_stack.push_back(root);
    }
    while (!m_stack.empty())
    {
        const uint32_t node = m_stack.back();
        m_stack.pop_back();
        ++m_reachable;
        m_max_depth = math::max(m_max_depth, m_depth[node]);
        if (m_depth[node] + 1 >= k_max_tree_depth)
        {
            continue;
        }
        for (uint32_t child = m_first_child[node]; child != k_no_tree_node; child = m_next_sibling[child])
        {
            m_depth[child] = m_depth[node] + 1;
            m_stack.push_back(child);
        }
    }

    for (uint32_t node = m_best_child[m_count]; node != k_no_tree_node && reachable(node) && m_principal[node] == 0; node = m_best_child[node])
    {
        m_principal[node] = 1;
    }
}

double TreeIndex::share_of_parent(uint32_t node) const
{
    const double total = m_group_visits[m_parent[node]];
    return total > 0.0 ? static_cast<double>(m_nodes[node].visits) / total : std::numeric_limits<double>::quiet_NaN();
}

void TreeIndex::visible_rows(const uint8_t* expanded, std::vector<uint32_t>& rows) const
{
    rows.clear();
    uint32_t node = m_count == 0 ? k_no_tree_node : m_first_child[m_count];
    while (node != k_no_tree_node)
    {
        if (reachable(node))
        {
            rows.push_back(node);
            const uint32_t child = m_first_child[node];
            if (expanded[node] != 0 && child != k_no_tree_node && reachable(child))
            {
                node = child;
                continue;
            }
        }
        while (node != k_no_tree_node && m_next_sibling[node] == k_no_tree_node)
        {
            node = m_parent[node] == m_count ? k_no_tree_node : m_parent[node];
        }
        node = node == k_no_tree_node ? k_no_tree_node : m_next_sibling[node];
    }
}

} // namespace oryx
