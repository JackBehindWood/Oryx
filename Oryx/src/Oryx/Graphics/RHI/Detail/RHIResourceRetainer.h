#pragma once

#include "Oryx/Containers/SmallVector.h"
#include "Oryx/Graphics/RHI/RHIBinding.h"
#include "Oryx/Graphics/RHI/RHIRenderState.h"
#include "Oryx/Graphics/RHI/RHIResource.h"

namespace oryx
{

// Holds a reference to every resource a recording names so the caller may drop its own before submit.
class RHIResourceRetainer
{
public:
    static constexpr uint32_t SLOT_TARGET = 0;
    static constexpr uint32_t SLOT_DEPTH = SLOT_TARGET + RHI_MAX_COLOUR_TARGETS;
    static constexpr uint32_t SLOT_PIPELINE = SLOT_DEPTH + 1;
    static constexpr uint32_t SLOT_INDEX = SLOT_PIPELINE + 1;
    static constexpr uint32_t SLOT_VERTEX = SLOT_INDEX + 1;
    static constexpr uint32_t SLOT_COUNT = SLOT_VERTEX + RHI_MAX_VERTEX_SLOTS;
    static constexpr size_t SCAN_WINDOW = 16;

    RHIResourceRetainer() = default;
    RHIResourceRetainer(const RHIResourceRetainer&) = delete;
    RHIResourceRetainer& operator=(const RHIResourceRetainer&) = delete;
    RHIResourceRetainer(RHIResourceRetainer&& other) noexcept;
    RHIResourceRetainer& operator=(RHIResourceRetainer&& other) noexcept;

    // Skips a resource found among the last SCAN_WINDOW retained.
    void retain(RHIResource& resource);
    // Skips when `resource` is what the slot or binding last retained.
    void retain_in_slot(uint32_t slot, RHIResource& resource);
    void retain_for_binding(RHIBindingId binding, RHIResource& resource);

    // Moves the references into `sink` and clears.
    void drain_into(std::vector<Ref<RHIResource>>& sink);
    void clear();

    [[nodiscard]] size_t size() const { return m_retained.size(); }

private:
    SmallVector<Ref<RHIResource>, 16> m_retained;
    const RHIResource* m_last_in_slot[SLOT_COUNT] = {};
    const RHIResource* m_last_binding[RHI_MAX_BINDINGS] = {};
};

} // namespace oryx
