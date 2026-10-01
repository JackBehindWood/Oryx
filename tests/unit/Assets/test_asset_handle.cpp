#include "doctest.h"

#include "Oryx.h"

namespace
{
struct Tag
{
};
} // namespace

TEST_CASE("default AssetId and AssetHandle are null")
{
    CHECK(oryx::is_null(oryx::AssetId{}));
    CHECK(oryx::is_null(oryx::AssetHandle<Tag>{}));
}

TEST_CASE("AssetId compares by value")
{
    CHECK(oryx::AssetId{ 3 } == oryx::AssetId{ 3 });
    CHECK(oryx::AssetId{ 3 } != oryx::AssetId{ 4 });
    CHECK_FALSE(oryx::is_null(oryx::AssetId{ 1 }));
}

TEST_CASE("AssetHandle differs when id or generation differs")
{
    oryx::AssetHandle<Tag> a{ oryx::AssetId{ 1 }, 1 };
    CHECK(a == oryx::AssetHandle<Tag>{ oryx::AssetId{ 1 }, 1 });
    CHECK(a != oryx::AssetHandle<Tag>{ oryx::AssetId{ 2 }, 1 });
    CHECK(a != oryx::AssetHandle<Tag>{ oryx::AssetId{ 1 }, 2 });
}
