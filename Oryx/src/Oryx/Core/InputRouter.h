#pragma once

#include "Oryx/Core/Input.h"
#include "Oryx/Core/ViewRegion.h"

namespace oryx
{

enum class PointerOwner : uint8_t
{
    None,
    Interface,
    World
};

// Decides, once per frame, whether the interface (menus, panels) or a world view (a board) owns the pointer and keyboard, and hands each a filtered view of the window's input.
// Per frame: begin_frame, interface clients claim and set views, begin_world, world clients read. A press is owned by whoever it began on until every button is up.
class InputRouter
{
public:
    InputRouter();
    InputRouter(const InputRouter&) = delete;
    InputRouter& operator=(const InputRouter&) = delete;

    // Resets claims and views (the main view is the whole `surface`) and ends a finished latch.
    void begin_frame(const IInput& input, const Vec2f& surface);
    // Throws Error for an id at or beyond k_max_views.
    void set_view(ViewId view, const ViewRegion& region);
    void claim_pointer() { m_pointer_claimed = true; }
    void claim_keyboard() { m_keyboard_claimed = true; }
    // Ends the interface phase: a press with no owner is given to the interface when claimed or outside every view, else to the view under it.
    void begin_world();

    [[nodiscard]] const IInput& interface_input() const { return m_interface; }
    // Cursor in the view's own coordinates; throws Error for an id at or beyond k_max_views.
    [[nodiscard]] const IInput& world_input(ViewId view) const;

    [[nodiscard]] PointerOwner pointer_owner() const { return m_owner; }
    [[nodiscard]] ViewId owner_view() const { return m_owner_view; }
    [[nodiscard]] bool pointer_claimed() const { return m_pointer_claimed; }
    [[nodiscard]] bool keyboard_claimed() const { return m_keyboard_claimed; }
    [[nodiscard]] bool has_view(ViewId view) const { return view < k_max_views && m_defined[view]; }
    [[nodiscard]] const ViewRegion& view(ViewId view) const;

private:
    class Filtered final : public IInput
    {
    public:
        Filtered(const InputRouter& router, bool world, ViewId view)
            : m_router(router)
            , m_world(world)
            , m_view(view)
        {
        }

        [[nodiscard]] bool key_down(KeyCode key) const override;
        [[nodiscard]] bool key_pressed(KeyCode key) const override;
        [[nodiscard]] bool key_released(KeyCode key) const override;
        [[nodiscard]] bool mouse_down(MouseCode button) const override;
        [[nodiscard]] bool mouse_pressed(MouseCode button) const override;
        [[nodiscard]] bool mouse_released(MouseCode button) const override;
        void cursor_position(Vec2f& out) const override;
        void scroll_delta(Vec2f& out) const override;
        [[nodiscard]] std::string_view typed_text() const override;
        [[nodiscard]] bool key_repeated(KeyCode key) const override;
        [[nodiscard]] std::string_view paste_text() const override;

    private:
        [[nodiscard]] bool keys_visible() const;
        [[nodiscard]] bool buttons_visible() const;
        [[nodiscard]] bool hover_visible() const;

        const InputRouter& m_router;
        bool m_world;
        ViewId m_view;
    };

    // The first view whose region holds the cursor, or k_max_views.
    [[nodiscard]] ViewId view_under_cursor() const;

    const IInput* m_input = nullptr;
    ViewRegion m_regions[k_max_views];
    bool m_defined[k_max_views] = {};
    PointerOwner m_owner = PointerOwner::None;
    ViewId m_owner_view = k_main_view;
    ViewId m_hover_view = k_max_views;
    bool m_pointer_claimed = false;
    bool m_keyboard_claimed = false;
    Filtered m_interface;
    std::vector<Filtered> m_world;
};

} // namespace oryx
