#pragma once

#include "Oryx/Interface/GUI/Dock/DockOps.h"

// Describes a whole layout in code by panel name: split nodes, dock panels into them, then build once against the registered panels. A reference stays valid after the node is split (it keeps the side it had).
namespace oryx::gui
{

struct DockNodeRef
{
    int32_t index = -1;
};

class DockBuilder
{
public:
    DockBuilder();

    // The tree's root; the first node to dock into or split.
    [[nodiscard]] DockNodeRef root() const { return DockNodeRef{ m_root }; }
    // Splits a tab stack: `node` keeps its side, the returned node takes `zone`'s side. points > 0 fixes the new side's size in points (it does not rescale with the window), else it takes a ratio.
    [[nodiscard]] DockNodeRef split(DockNodeRef node, DropZone zone, float points = 0.0f);
    // Tabs keep the order they are docked in and the last one is selected, as when a panel is docked by hand.
    DockBuilder& dock(DockNodeRef node, std::string_view panel);
    DockBuilder& collapse(DockNodeRef node, bool collapsed = true);

    // Same result the registration hints give for the same intent. Throws oryx::Error naming the panel or node at fault (unknown or repeated panel, a split used as a stack, too many tabs or nodes).
    [[nodiscard]] DockLayout build(const PanelTable& panels) const;
    // Against the active GuiContext's registered panels.
    [[nodiscard]] DockLayout build() const;

private:
    struct Entry
    {
        bool is_split = false;
        int32_t parent = -1;
        int32_t first = -1;
        int32_t second = -1;
        DropZone zone = DropZone::Right;
        float points = 0.0f;
        bool collapsed = false;
        std::vector<std::string> panels;
    };

    [[nodiscard]] Entry& leaf(DockNodeRef node, const char* verb);
    int32_t emit(const PanelTable& panels, DockLayout& out, int32_t index) const;

    std::vector<Entry> m_entries;
    int32_t m_root = 0;
};

}
