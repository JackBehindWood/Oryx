#include "doctest.h"

#include "Oryx.h"

namespace
{

struct DummyAsset
{
    explicit DummyAsset(std::string name_in)
        : name(std::move(name_in))
    {
        ++live;
    }
    DummyAsset(DummyAsset&&) = delete;
    ~DummyAsset() { --live; }

    std::string name;
    static inline int32_t live = 0;
};

struct OtherAsset
{
};

oryx::AssetManager* g_manager = nullptr;
int32_t g_load_calls = 0;
oryx::AssetState g_state_during_load = oryx::AssetState::Ready;
bool g_nested_load = false;
oryx::AssetHandle<DummyAsset> g_nested_handle;

class DummyLoader : public oryx::IAssetLoader<DummyAsset>
{
public:
    oryx::UniquePtr<DummyAsset> load(const std::filesystem::path& path) const override
    {
        ++g_load_calls;
        std::string name = path.generic_string();
        if (name.find("bad") != std::string::npos)
        {
            throw oryx::Error("bad asset", "detail text");
        }
        if (name.find("probe") != std::string::npos && g_manager != nullptr)
        {
            oryx::AssetHandle<DummyAsset> self = g_manager->find<DummyAsset>(path);
            g_state_during_load = g_manager->state(self);
        }
        if (name.find("nested") != std::string::npos && g_manager != nullptr)
        {
            g_nested_handle = g_manager->load<DummyAsset>("inner.dummy");
        }
        return oryx::create_unique<DummyAsset>(name);
    }
};

class OtherLoader : public oryx::IAssetLoader<OtherAsset>
{
public:
    oryx::UniquePtr<OtherAsset> load(const std::filesystem::path&) const override { return oryx::create_unique<OtherAsset>(); }
};

OX_REGISTER_ASSET_LOADER(DummyAsset, DummyLoader, ".dummy")
OX_REGISTER_ASSET_LOADER(OtherAsset, OtherLoader, ".dummy")

struct Fixture
{
    Fixture()
    {
        g_load_calls = 0;
        g_manager = nullptr;
    }
};

} // namespace

TEST_CASE_FIXTURE(Fixture, "AssetManager::load produces a Ready asset")
{
    oryx::AssetManager manager;
    oryx::AssetHandle<DummyAsset> handle = manager.load<DummyAsset>("a.dummy");
    CHECK_FALSE(oryx::is_null(handle));
    CHECK(manager.state(handle) == oryx::AssetState::Ready);
    CHECK(manager.get(handle).name == "a.dummy");
    CHECK(manager.try_get(handle) != nullptr);
    CHECK(manager.ref_count(handle) == 1);
    CHECK(manager.size<DummyAsset>() == 1);
}

TEST_CASE_FIXTURE(Fixture, "AssetManager caches by path and ref-counts")
{
    oryx::AssetManager manager;
    oryx::AssetHandle<DummyAsset> first = manager.load<DummyAsset>("a.dummy");
    oryx::AssetHandle<DummyAsset> second = manager.load<DummyAsset>("a.dummy");
    CHECK(first == second);
    CHECK(g_load_calls == 1);
    CHECK(manager.ref_count(first) == 2);
    CHECK(manager.find<DummyAsset>("a.dummy") == first);
}

TEST_CASE_FIXTURE(Fixture, "AssetManager treats equivalent paths as one asset")
{
    oryx::AssetManager manager;
    oryx::AssetHandle<DummyAsset> a = manager.load<DummyAsset>("dir/./a.dummy");
    oryx::AssetHandle<DummyAsset> b = manager.load<DummyAsset>("dir/a.dummy");
    CHECK(a == b);
    CHECK(g_load_calls == 1);
}

TEST_CASE_FIXTURE(Fixture, "AssetManager gives distinct paths distinct ids")
{
    oryx::AssetManager manager;
    oryx::AssetHandle<DummyAsset> a = manager.load<DummyAsset>("a.dummy");
    oryx::AssetHandle<DummyAsset> b = manager.load<DummyAsset>("b.dummy");
    CHECK(a.id != b.id);
    CHECK(manager.size<DummyAsset>() == 2);
}

TEST_CASE_FIXTURE(Fixture, "AssetManager resolves loaders by case-insensitive extension")
{
    oryx::AssetManager manager;
    oryx::AssetHandle<DummyAsset> handle = manager.load<DummyAsset>("A.DUMMY");
    CHECK(manager.state(handle) == oryx::AssetState::Ready);
}

TEST_CASE_FIXTURE(Fixture, "AssetManager::release unloads at zero references")
{
    oryx::AssetManager manager;
    int32_t before = DummyAsset::live;
    oryx::AssetHandle<DummyAsset> handle = manager.load<DummyAsset>("a.dummy");
    manager.load<DummyAsset>("a.dummy");
    CHECK(DummyAsset::live == before + 1);

    manager.release(handle);
    CHECK(manager.alive(handle));
    CHECK(manager.ref_count(handle) == 1);

    manager.release(handle);
    CHECK_FALSE(manager.alive(handle));
    CHECK(oryx::is_null(manager.find<DummyAsset>("a.dummy")));
    CHECK(manager.try_get(handle) == nullptr);
    CHECK_THROWS_AS(manager.get(handle), oryx::Error);
    CHECK(DummyAsset::live == before);
    CHECK(manager.size<DummyAsset>() == 0);
}

TEST_CASE_FIXTURE(Fixture, "AssetManager reuses slots with a new generation")
{
    oryx::AssetManager manager;
    oryx::AssetHandle<DummyAsset> old_handle = manager.load<DummyAsset>("a.dummy");
    manager.release(old_handle);
    oryx::AssetHandle<DummyAsset> fresh = manager.load<DummyAsset>("b.dummy");
    CHECK(fresh.id == old_handle.id);
    CHECK(fresh.generation != old_handle.generation);
    CHECK_FALSE(manager.alive(old_handle));
    CHECK(manager.alive(fresh));
}

TEST_CASE_FIXTURE(Fixture, "AssetManager marks a throwing loader Failed")
{
    oryx::AssetManager manager;
    oryx::AssetHandle<DummyAsset> handle = manager.load<DummyAsset>("bad.dummy");
    CHECK(manager.state(handle) == oryx::AssetState::Failed);
    CHECK(manager.error(handle).find("bad asset") != std::string::npos);
    CHECK(manager.error(handle).find("detail text") != std::string::npos);
    CHECK(manager.try_get(handle) == nullptr);
    CHECK_THROWS_AS(manager.get(handle), oryx::Error);
}

TEST_CASE_FIXTURE(Fixture, "AssetManager marks an unknown extension Failed")
{
    oryx::AssetManager manager;
    oryx::AssetHandle<DummyAsset> handle = manager.load<DummyAsset>("a.unknown-extension");
    CHECK(manager.state(handle) == oryx::AssetState::Failed);
    CHECK(manager.error(handle).find("no loader") != std::string::npos);
}

TEST_CASE_FIXTURE(Fixture, "AssetManager caches a failure until released, then retries")
{
    oryx::AssetManager manager;
    oryx::AssetHandle<DummyAsset> handle = manager.load<DummyAsset>("bad.dummy");
    CHECK(manager.load<DummyAsset>("bad.dummy") == handle);
    CHECK(g_load_calls == 1);

    manager.release(handle);
    manager.release(handle);
    manager.load<DummyAsset>("bad.dummy");
    CHECK(g_load_calls == 2);
}

TEST_CASE_FIXTURE(Fixture, "AssetManager exposes Loading while the loader runs")
{
    oryx::AssetManager manager;
    g_manager = &manager;
    g_state_during_load = oryx::AssetState::Ready;
    oryx::AssetHandle<DummyAsset> handle = manager.load<DummyAsset>("probe.dummy");
    CHECK(g_state_during_load == oryx::AssetState::Loading);
    CHECK(manager.state(handle) == oryx::AssetState::Ready);
}

TEST_CASE_FIXTURE(Fixture, "AssetManager allows a loader to load other assets")
{
    oryx::AssetManager manager;
    g_manager = &manager;
    oryx::AssetHandle<DummyAsset> outer = manager.load<DummyAsset>("nested.dummy");
    CHECK(manager.state(outer) == oryx::AssetState::Ready);
    CHECK(manager.state(g_nested_handle) == oryx::AssetState::Ready);
    CHECK(manager.get(g_nested_handle).name == "inner.dummy");
}

TEST_CASE_FIXTURE(Fixture, "AssetManager keeps one table per asset type")
{
    oryx::AssetManager manager;
    oryx::AssetHandle<DummyAsset> dummy = manager.load<DummyAsset>("a.dummy");
    oryx::AssetHandle<OtherAsset> other = manager.load<OtherAsset>("a.dummy");
    CHECK(manager.state(dummy) == oryx::AssetState::Ready);
    CHECK(manager.state(other) == oryx::AssetState::Ready);
    CHECK(manager.size<DummyAsset>() == 1);
    CHECK(manager.size<OtherAsset>() == 1);
}

TEST_CASE_FIXTURE(Fixture, "AssetManager::release rejects stale and null handles")
{
    oryx::AssetManager manager;
    oryx::AssetHandle<DummyAsset> handle = manager.load<DummyAsset>("a.dummy");
    manager.release(handle);
    CHECK_THROWS_AS(manager.release(handle), oryx::Error);
    CHECK_THROWS_AS(manager.release(oryx::AssetHandle<DummyAsset>{}), oryx::Error);
    CHECK_THROWS_AS(manager.state(oryx::AssetHandle<DummyAsset>{}), oryx::Error);
}

TEST_CASE_FIXTURE(Fixture, "AssetManager destroys live assets with itself")
{
    int32_t before = DummyAsset::live;
    {
        oryx::AssetManager manager;
        manager.load<DummyAsset>("a.dummy");
        manager.load<DummyAsset>("b.dummy");
        CHECK(DummyAsset::live == before + 2);
    }
    CHECK(DummyAsset::live == before);
}
