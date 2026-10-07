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

TEST_CASE("ImId: the hash is pinned so saved layouts survive refactors")
{
    CHECK(make_im_id("").value == 0xa8c7f832281a39c5ull);
    CHECK(make_im_id("button").value == 0x502ec91b7d442111ull);
    CHECK(make_im_id("ok", make_im_id("panel")).value == 0x61b756d8164fa42bull);
    CHECK(make_im_index_id(0).value == 0x2a17b88efd5cab30ull);
    CHECK(make_im_index_id(3, make_im_id("panel")).value == 0x1622d04c899a398bull);
}

TEST_CASE("FrameArena::format is locale-neutral")
{
    const char* const candidates[] = { "de_DE.UTF-8", "fr_FR.UTF-8", "nl_NL.UTF-8" };
    const std::string saved = std::setlocale(LC_ALL, nullptr);
    bool switched = false;
    for (const char* name : candidates)
    {
        if (std::setlocale(LC_ALL, name) != nullptr)
        {
            switched = true;
            break;
        }
    }
    FrameArena arena;
    CHECK(arena.format("%.2f", 1.5) == "1.50");
    std::setlocale(LC_ALL, saved.c_str());
    if (!switched)
    {
        MESSAGE("no comma-decimal locale installed; checked the default locale only");
    }
}
