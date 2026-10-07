#include "doctest.h"

#include "Oryx.h"

using namespace oryx;

static_assert(!std::is_convertible_v<UiId, GuiId>);
static_assert(!std::is_convertible_v<GuiId, UiId>);
static_assert(!std::is_convertible_v<ImId, UiId>);
static_assert(!std::is_convertible_v<ImId, GuiId>);
static_assert(!std::is_convertible_v<UiId, ImId>);
static_assert(!std::is_convertible_v<GuiId, ImId>);
static_assert(std::is_trivially_copyable_v<UiId> && std::is_standard_layout_v<UiId>);
static_assert(sizeof(UiId) == sizeof(uint64_t) && sizeof(GuiId) == sizeof(uint64_t));

namespace
{

struct SeededContext : ImContext
{
    explicit SeededContext(uint64_t seed)
        : ImContext(seed)
    {
    }
};

} // namespace

TEST_CASE("TypedId: the explicit door round-trips and compares")
{
    const ImId raw = make_im_id("a");
    const UiId ui(raw);
    CHECK(ui.im() == raw);
    CHECK(ui == UiId(raw));
    CHECK(ui != UiId(make_im_id("b")));
    CHECK(is_valid(ui));
    CHECK(!is_valid(UiId{}));
}

TEST_CASE("ImContext: the id seed keeps context kinds apart")
{
    SeededContext plain(0);
    SeededContext other(k_ui_id_seed);
    UiContext ui;
    CHECK(plain.id("x") == make_im_id("x"));
    CHECK(plain.id("x") != other.id("x"));
    CHECK(other.id("x") == ui.id("x"));
    CHECK(plain.index_id(3) != other.index_id(3));
}
