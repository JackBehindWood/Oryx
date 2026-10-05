#include "doctest.h"

#include "AssetTestSupport.h"

using namespace oryx;

namespace
{

struct DummyAsset : Asset
{
    explicit DummyAsset(std::string name_in)
        : name(std::move(name_in))
    {
        ++live;
    }
    DummyAsset(DummyAsset&&) = delete;
    ~DummyAsset() { --live; }

    size_t memory_bytes() const override { return name.size(); }

    std::string name;
    static inline int32_t live = 0;
};

struct OtherAsset : Asset
{
    size_t memory_bytes() const override { return 0; }
};

AssetManager* g_manager = nullptr;
int32_t g_import_calls = 0;
std::vector<std::string> g_import_order;
AssetHandle<DummyAsset> g_nested_handle;

std::string text_of(const uint8_t* bytes, size_t size)
{
    return std::string(reinterpret_cast<const char*>(bytes), size);
}

class DummyImporter : public IAssetImporter<DummyAsset>
{
public:
    std::string_view id() const override { return "dummy"; }
    uint32_t version() const override { return 1; }

    std::vector<uint8_t> import_source(const ImportInput& input) const override
    {
        ++g_import_calls;
        g_import_order.push_back(input.path.filename().generic_string());
        std::string text = text_of(input.bytes, input.size);
        if (text.find("bad") != std::string::npos)
        {
            throw Error("bad asset", "detail text");
        }
        return std::vector<uint8_t>(input.bytes, input.bytes + input.size);
    }

    UniquePtr<DummyAsset> instantiate(const uint8_t* payload, size_t size) const override
    {
        std::string text = text_of(payload, size);
        if (text.rfind("nested:", 0) == 0 && g_manager != nullptr)
        {
            g_nested_handle = g_manager->load<DummyAsset>(text.substr(7));
        }
        return create_unique<DummyAsset>(text);
    }
};

class OtherImporter : public IAssetImporter<OtherAsset>
{
public:
    std::string_view id() const override { return "other"; }
    uint32_t version() const override { return 1; }
    std::vector<uint8_t> import_source(const ImportInput&) const override { return {}; }
    UniquePtr<OtherAsset> instantiate(const uint8_t*, size_t) const override { return create_unique<OtherAsset>(); }
};

OX_REGISTER_ASSET_IMPORTER(DummyAsset, DummyImporter, ".dummy")
OX_REGISTER_ASSET_IMPORTER(OtherAsset, OtherImporter, ".dummy")

class RecordingManager : public AssetManager
{
public:
    using AssetManager::AssetManager;

    std::filesystem::path resolve(const std::filesystem::path& path) const { return resolve_path(path); }

    std::vector<AssetId> reloaded;

protected:
    void on_reloaded(std::type_index, AssetId id) override { reloaded.push_back(id); }
};

struct Fixture
{
    Fixture()
    {
        g_import_calls = 0;
        g_import_order.clear();
        g_manager = nullptr;
        g_nested_handle = {};
    }

    std::filesystem::path file(const std::string& name, const std::string& content)
    {
        std::filesystem::path path = dir.path() / name;
        std::filesystem::create_directories(path.parent_path());
        test::write_bytes(path, std::vector<uint8_t>(content.begin(), content.end()));
        return path;
    }

    test::AssetTempDir dir;
    AssetManager manager{ test::uncached_settings() };
};

} // namespace

TEST_CASE_FIXTURE(Fixture, "AssetManager::load is Loading until update, then Ready")
{
    AssetHandle<DummyAsset> handle = manager.load<DummyAsset>(file("a.dummy", "alpha"));
    CHECK_FALSE(is_null(handle));
    CHECK(manager.state(handle) == AssetState::Loading);
    CHECK(manager.try_get(handle) == nullptr);
    CHECK(manager.pending(handle));
    CHECK(g_import_calls == 0);

    manager.update();
    CHECK(manager.state(handle) == AssetState::Ready);
    CHECK_FALSE(manager.pending(handle));
    CHECK(manager.get(handle).name == "alpha");
    CHECK(manager.ref_count(handle) == 1);
    CHECK(manager.revision(handle) == 1);
    CHECK(manager.size<DummyAsset>() == 1);
}

TEST_CASE_FIXTURE(Fixture, "AssetManager records the resolved source path on the asset")
{
    std::filesystem::path path = file("a.dummy", "alpha");
    AssetHandle<DummyAsset> handle = test::load_now<DummyAsset>(manager, path);
    CHECK(manager.get(handle).source_path() == path);
    CHECK(manager.get(handle).memory_bytes() == 5);
}

TEST_CASE_FIXTURE(Fixture, "AssetManager caches by path and ref-counts")
{
    std::filesystem::path path = file("a.dummy", "alpha");
    AssetHandle<DummyAsset> first = manager.load<DummyAsset>(path);
    AssetHandle<DummyAsset> second = manager.load<DummyAsset>(path);
    manager.wait(first);
    CHECK(first == second);
    CHECK(g_import_calls == 1);
    CHECK(manager.ref_count(first) == 2);
    CHECK(manager.find<DummyAsset>(path) == first);
}

TEST_CASE_FIXTURE(Fixture, "AssetManager treats equivalent paths as one asset")
{
    file("dir/a.dummy", "alpha");
    AssetHandle<DummyAsset> a = manager.load<DummyAsset>(dir.path() / "dir/./a.dummy");
    AssetHandle<DummyAsset> b = manager.load<DummyAsset>(dir.path() / "dir/a.dummy");
    manager.wait(a);
    CHECK(a == b);
    CHECK(g_import_calls == 1);
}

TEST_CASE_FIXTURE(Fixture, "AssetManager gives distinct paths distinct ids")
{
    AssetHandle<DummyAsset> a = manager.load<DummyAsset>(file("a.dummy", "a"));
    AssetHandle<DummyAsset> b = manager.load<DummyAsset>(file("b.dummy", "b"));
    CHECK(a.id != b.id);
    CHECK(manager.size<DummyAsset>() == 2);
}

TEST_CASE_FIXTURE(Fixture, "AssetManager resolves importers by case-insensitive extension")
{
    AssetHandle<DummyAsset> handle = test::load_now<DummyAsset>(manager, file("A.DUMMY", "x"));
    CHECK(manager.state(handle) == AssetState::Ready);
}

TEST_CASE_FIXTURE(Fixture, "AssetManager::release unloads at zero references")
{
    std::filesystem::path path = file("a.dummy", "alpha");
    int32_t before = DummyAsset::live;
    AssetHandle<DummyAsset> handle = test::load_now<DummyAsset>(manager, path);
    manager.load<DummyAsset>(path);
    CHECK(DummyAsset::live == before + 1);

    manager.release(handle);
    CHECK(manager.alive(handle));
    CHECK(manager.ref_count(handle) == 1);

    manager.release(handle);
    CHECK_FALSE(manager.alive(handle));
    CHECK(is_null(manager.find<DummyAsset>(path)));
    CHECK(manager.try_get(handle) == nullptr);
    CHECK_THROWS_AS(manager.get(handle), Error);
    CHECK(DummyAsset::live == before);
    CHECK(manager.size<DummyAsset>() == 0);
}

TEST_CASE_FIXTURE(Fixture, "AssetManager reuses slots with a new generation")
{
    AssetHandle<DummyAsset> old_handle = test::load_now<DummyAsset>(manager, file("a.dummy", "a"));
    manager.release(old_handle);
    AssetHandle<DummyAsset> fresh = test::load_now<DummyAsset>(manager, file("b.dummy", "b"));
    CHECK(fresh.id == old_handle.id);
    CHECK(fresh.generation != old_handle.generation);
    CHECK_FALSE(manager.alive(old_handle));
    CHECK(manager.alive(fresh));
    CHECK(manager.revision(fresh) == 1);
}

TEST_CASE_FIXTURE(Fixture, "AssetManager marks a throwing importer Failed")
{
    AssetHandle<DummyAsset> handle = test::load_now<DummyAsset>(manager, file("bad.dummy", "bad"));
    CHECK(manager.state(handle) == AssetState::Failed);
    CHECK(manager.error(handle).find("bad asset") != std::string::npos);
    CHECK(manager.error(handle).find("detail text") != std::string::npos);
    CHECK(manager.try_get(handle) == nullptr);
    CHECK_THROWS_AS(manager.get(handle), Error);
}

TEST_CASE_FIXTURE(Fixture, "AssetManager marks an unknown extension and a missing file Failed")
{
    AssetHandle<DummyAsset> unknown = test::load_now<DummyAsset>(manager, file("a.unknown-extension", "x"));
    CHECK(manager.state(unknown) == AssetState::Failed);
    CHECK(manager.error(unknown).find("no importer") != std::string::npos);

    AssetHandle<DummyAsset> missing = test::load_now<DummyAsset>(manager, dir.path() / "missing.dummy");
    CHECK(manager.state(missing) == AssetState::Failed);
}

TEST_CASE_FIXTURE(Fixture, "AssetManager caches a failure until released, then retries")
{
    std::filesystem::path path = file("bad.dummy", "bad");
    AssetHandle<DummyAsset> handle = test::load_now<DummyAsset>(manager, path);
    CHECK(manager.load<DummyAsset>(path) == handle);
    manager.update();
    CHECK(g_import_calls == 1);

    manager.release(handle);
    manager.release(handle);
    test::load_now<DummyAsset>(manager, path);
    CHECK(g_import_calls == 2);
}

TEST_CASE_FIXTURE(Fixture, "AssetManager runs requests first in first out")
{
    AssetHandle<DummyAsset> a = manager.load<DummyAsset>(file("1.dummy", "1"));
    AssetHandle<DummyAsset> b = manager.load<DummyAsset>(file("2.dummy", "2"));
    AssetHandle<DummyAsset> c = manager.load<DummyAsset>(file("3.dummy", "3"));
    manager.update();
    CHECK(g_import_order == std::vector<std::string>{ "1.dummy", "2.dummy", "3.dummy" });
    CHECK(manager.state(c) == AssetState::Ready);
    (void)a;
    (void)b;
}

TEST_CASE_FIXTURE(Fixture, "AssetManager publishes at most the budgeted requests per update")
{
    AssetHandle<DummyAsset> a = manager.load<DummyAsset>(file("1.dummy", "1"));
    AssetHandle<DummyAsset> b = manager.load<DummyAsset>(file("2.dummy", "2"));
    AssetHandle<DummyAsset> c = manager.load<DummyAsset>(file("3.dummy", "3"));
    manager.set_budget({ .max_requests = 2 });

    manager.update();
    CHECK(manager.state(a) == AssetState::Ready);
    CHECK(manager.state(b) == AssetState::Ready);
    CHECK(manager.state(c) == AssetState::Loading);

    manager.update();
    CHECK(manager.state(c) == AssetState::Ready);
}

TEST_CASE_FIXTURE(Fixture, "AssetManager::wait pumps until the handle settles")
{
    AssetHandle<DummyAsset> handle = manager.load<DummyAsset>(file("a.dummy", "alpha"));
    manager.wait(handle);
    CHECK(manager.state(handle) == AssetState::Ready);
    CHECK_THROWS_AS(manager.wait(AssetHandle<DummyAsset>{}), Error);
}

TEST_CASE_FIXTURE(Fixture, "AssetManager::on_ready runs from update, also when already settled")
{
    AssetHandle<DummyAsset> handle = manager.load<DummyAsset>(file("a.dummy", "alpha"));
    int32_t calls = 0;
    AssetHandle<DummyAsset> seen;
    manager.on_ready<DummyAsset>(handle, [&](AssetHandle<DummyAsset> ready) { ++calls; seen = ready; });
    CHECK(calls == 0);
    manager.update();
    CHECK(calls == 1);
    CHECK(seen == handle);

    manager.on_ready<DummyAsset>(handle, [&](AssetHandle<DummyAsset>) { ++calls; });
    CHECK(calls == 1);
    manager.update();
    CHECK(calls == 2);
}

TEST_CASE_FIXTURE(Fixture, "AssetManager allows instantiate to load other assets")
{
    std::filesystem::path inner = file("inner.dummy", "inner");
    g_manager = &manager;
    AssetHandle<DummyAsset> outer = manager.load<DummyAsset>(file("outer.dummy", "nested:" + inner.generic_string()));
    manager.wait(outer);
    REQUIRE_FALSE(is_null(g_nested_handle));
    manager.wait(g_nested_handle);
    CHECK(manager.state(outer) == AssetState::Ready);
    CHECK(manager.get(g_nested_handle).name == "inner");
}

TEST_CASE_FIXTURE(Fixture, "AssetManager allows an on_ready callback to load and wait on another asset")
{
    std::filesystem::path second = file("second.dummy", "second");
    AssetHandle<DummyAsset> first = manager.load<DummyAsset>(file("first.dummy", "first"));
    AssetHandle<DummyAsset> chained;
    manager.on_ready<DummyAsset>(first, [&](AssetHandle<DummyAsset>)
    {
        chained = manager.load<DummyAsset>(second);
        manager.wait(chained);
    });
    manager.update();
    REQUIRE_FALSE(is_null(chained));
    CHECK(manager.get(chained).name == "second");
}

TEST_CASE_FIXTURE(Fixture, "AssetManager::release cancels a queued request")
{
    AssetHandle<DummyAsset> handle = manager.load<DummyAsset>(file("a.dummy", "alpha"));
    manager.release(handle);
    manager.update();
    CHECK(g_import_calls == 0);
    CHECK_FALSE(manager.alive(handle));
    CHECK(manager.size<DummyAsset>() == 0);
}

TEST_CASE_FIXTURE(Fixture, "AssetManager keeps one table per asset type")
{
    std::filesystem::path path = file("a.dummy", "a");
    AssetHandle<DummyAsset> dummy = test::load_now<DummyAsset>(manager, path);
    AssetHandle<OtherAsset> other = test::load_now<OtherAsset>(manager, path);
    CHECK(manager.state(dummy) == AssetState::Ready);
    CHECK(manager.state(other) == AssetState::Ready);
    CHECK(manager.size<DummyAsset>() == 1);
    CHECK(manager.size<OtherAsset>() == 1);
}

TEST_CASE_FIXTURE(Fixture, "AssetManager::release rejects stale and null handles")
{
    AssetHandle<DummyAsset> handle = test::load_now<DummyAsset>(manager, file("a.dummy", "a"));
    manager.release(handle);
    CHECK_THROWS_AS(manager.release(handle), Error);
    CHECK_THROWS_AS(manager.release(AssetHandle<DummyAsset>{}), Error);
    CHECK_THROWS_AS(manager.state(AssetHandle<DummyAsset>{}), Error);
}

TEST_CASE_FIXTURE(Fixture, "AssetManager destroys live assets with itself")
{
    std::filesystem::path a = file("a.dummy", "a");
    std::filesystem::path b = file("b.dummy", "b");
    int32_t before = DummyAsset::live;
    {
        AssetManager local(test::uncached_settings());
        test::load_now<DummyAsset>(local, a);
        test::load_now<DummyAsset>(local, b);
        local.load<DummyAsset>(dir.path() / "never-run.dummy");
        CHECK(DummyAsset::live == before + 2);
    }
    CHECK(DummyAsset::live == before);
}

TEST_CASE_FIXTURE(Fixture, "AssetManager::reload keeps the handle, serves the old asset until update and bumps the revision")
{
    std::filesystem::path path = file("a.dummy", "old");
    RecordingManager recording(test::uncached_settings());
    AssetHandle<DummyAsset> handle = test::load_now<DummyAsset>(recording, path);
    const DummyAsset* before = &recording.get(handle);

    file("a.dummy", "new");
    recording.reload(handle);
    CHECK(recording.state(handle) == AssetState::Ready);
    CHECK(recording.get(handle).name == "old");
    CHECK(recording.revision(handle) == 1);
    CHECK(recording.pending(handle));

    recording.update();
    CHECK(recording.get(handle).name == "new");
    CHECK(&recording.get(handle) != before);
    CHECK(recording.revision(handle) == 2);
    CHECK(recording.alive(handle));
    CHECK(recording.reloaded == std::vector<AssetId>{ handle.id });
}

TEST_CASE_FIXTURE(Fixture, "AssetManager::reload keeps the old asset when the new source fails")
{
    std::filesystem::path path = file("a.dummy", "good");
    AssetHandle<DummyAsset> handle = test::load_now<DummyAsset>(manager, path);

    file("a.dummy", "bad");
    manager.reload(handle);
    manager.update();
    CHECK(manager.state(handle) == AssetState::Ready);
    CHECK(manager.get(handle).name == "good");
    CHECK(manager.revision(handle) == 1);
    CHECK(manager.error(handle).find("bad asset") != std::string::npos);

    file("a.dummy", "fixed");
    manager.reload(handle);
    manager.update();
    CHECK(manager.get(handle).name == "fixed");
    CHECK(manager.error(handle).empty());
    CHECK(manager.revision(handle) == 2);
}

TEST_CASE_FIXTURE(Fixture, "AssetManager::resolve_path searches the settings roots in order")
{
    file("first/a.dummy", "from-first");
    file("second/a.dummy", "from-second");
    file("second/only.dummy", "only-second");

    AssetSettings settings = test::uncached_settings();
    settings.roots = { dir.path() / "missing", dir.path() / "first", dir.path() / "second" };
    RecordingManager rooted(settings);

    CHECK(rooted.resolve("a.dummy") == dir.path() / "first" / "a.dummy");
    CHECK(rooted.resolve("only.dummy") == dir.path() / "second" / "only.dummy");
    CHECK(rooted.resolve("nowhere.dummy") == std::filesystem::path("nowhere.dummy"));
    CHECK(rooted.resolve(dir.path() / "second" / "a.dummy") == dir.path() / "second" / "a.dummy");

    AssetHandle<DummyAsset> handle = test::load_now<DummyAsset>(rooted, "a.dummy");
    CHECK(rooted.get(handle).name == "from-first");
}

namespace
{

class SubManager : public AssetManager
{
public:
    explicit SubManager(int32_t tag_in)
        : AssetManager(test::uncached_settings())
        , tag(tag_in)
    {
    }

    int32_t tag;
};

} // namespace

TEST_CASE_FIXTURE(Fixture, "Assets::use installs a manager subclass and refuses while assets are live")
{
    SubManager& sub = Assets::use<SubManager>(7);
    CHECK(&Assets::manager() == &sub);
    CHECK(Assets::manager_as<SubManager>().tag == 7);
    CHECK_THROWS_AS(Assets::manager_as<RecordingManager>(), Error);

    AssetHandle<DummyAsset> handle = Assets::load<DummyAsset>(file("a.dummy", "global"));
    CHECK(Assets::state(handle) == AssetState::Loading);
    Assets::update();
    CHECK(Assets::get(handle).name == "global");

    CHECK_THROWS_AS(Assets::use<SubManager>(8), Error);
    CHECK(Assets::manager_as<SubManager>().tag == 7);

    Assets::release(handle);
    SubManager& replaced = Assets::use<SubManager>(9);
    CHECK(replaced.tag == 9);

    Assets::use<AssetManager>(test::uncached_settings());
}
