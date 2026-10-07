#pragma once

#include "Oryx/Interface/Canvas/ImId.h"
#include "Oryx/Interface/Canvas/Rect.h"

namespace oryx
{

inline constexpr float k_unbounded = 3.0e38f;

enum class SizingKind : uint8_t
{
    Fit,
    Grow,
    Fixed,
    Percent
};

// One axis of a box. `value` is the pixels (Fixed), the fraction of the parent's content box (Percent) or the share of leftover space (Grow); Fit takes the size of the content. `min`/`max` clamp every kind.
struct Sizing
{
    SizingKind kind = SizingKind::Fit;
    float value = 0.0f;
    float min = 0.0f;
    float max = k_unbounded;
};

[[nodiscard]] constexpr Sizing fit(float min = 0.0f, float max = k_unbounded) { return { SizingKind::Fit, 0.0f, min, max }; }
[[nodiscard]] constexpr Sizing grow(float weight = 1.0f, float min = 0.0f, float max = k_unbounded) { return { SizingKind::Grow, weight, min, max }; }
[[nodiscard]] constexpr Sizing fixed(float pixels) { return { SizingKind::Fixed, pixels, 0.0f, k_unbounded }; }
[[nodiscard]] constexpr Sizing percent(float fraction) { return { SizingKind::Percent, fraction, 0.0f, k_unbounded }; }

enum class Direction : uint8_t
{
    Row,
    Column
};

enum class Align : uint8_t
{
    Start,
    Centre,
    End
};

enum class Overflow : uint8_t
{
    Visible,
    Clip
};

enum class AttachPoint : uint8_t
{
    TopLeft,
    TopCentre,
    TopRight,
    CentreLeft,
    Centre,
    CentreRight,
    BottomLeft,
    BottomCentre,
    BottomRight
};

enum class FloatTarget : uint8_t
{
    Parent,
    Root,
    Element
};

// A box taken out of the flow and placed by one point on itself against one point on its target; it neither takes space from its siblings nor counts toward its parent's fit size.
struct Floating
{
    bool enabled = false;
    AttachPoint element = AttachPoint::TopLeft;
    AttachPoint target_point = AttachPoint::TopLeft;
    FloatTarget target = FloatTarget::Parent;
    // Used when target is Element; the element must be laid out before this box in the tree order.
    ImId target_id;
    Vec2f offset{ 0.0f, 0.0f };
};

// Plain data so a loader, a builder or a live reload can produce it.
struct LayoutStyle
{
    Sizing width;
    Sizing height;
    Direction direction = Direction::Row;
    // Where the children sit in the content box on each axis when they leave room.
    Align align_x = Align::Start;
    Align align_y = Align::Start;
    Insets padding;
    float gap = 0.0f;
    // Width over height; derives the missing axis, or fits inside two flexible axes.
    float aspect_ratio = 0.0f;
    Overflow overflow = Overflow::Visible;
    // Z-order channel of the box's own paint; zero inherits the parent's.
    uint32_t channel = 0;
    Floating floating;
};

} // namespace oryx
