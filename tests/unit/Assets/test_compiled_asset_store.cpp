#include "doctest.h"

#include "AssetTestSupport.h"
#include "unit/TestLogCapture.h"

using namespace oryx;

namespace
{

struct CountedAsset : Asset
{
    explicit CountedAsset(std::string text_in)
        : text(std::move(text_in))
    {
    }

    size_t memory_bytes() const override { return text.size(); }

    std::string text;
};

int32_t g_imports = 0;
int32_t g_instantiations = 0;
uint32_t g_version = 1;
uint64_t g_settings = 0;

class CountedImporter : public IAssetImporter<CountedAsset>
{
public:
    std::string_view id() const override { return "counted"; }
    uint32_t version() const override { return g_version; }
    uint64_t settings_hash() const override { return g_settings; }

    std::vector<uint8_t> import_source(const ImportInput& input) const override
    {
        ++g_imports;
        std::vector<uint8_t> payload(input.bytes, input.bytes + input.size);
        std::reverse(payload.begin(), payload.end());
        return payload;
    }

    UniquePtr<CountedAsset> instantiate(const uint8_t* payload, size_t size) const override
    {
        ++g_instantiations;
        return create_unique<CountedAsset>(std::string(reinterpret_cast<const char*>(payload), size));
    }
};

OX_REGISTER_ASSET_IMPORTER(CountedAsset, CountedImporter, ".counted")

struct CacheFixture
{
    CacheFixture()
    {
        g_imports = 0;
        g_instantiations = 0;
        g_version = 1;
        g_settings = 0;
    }

    std::filesystem::path source(const std::string& name, const std::string& content)
    {
        std::filesystem::path path = dir.path() / "src" / name;
        std::filesystem::create_directories(path.parent_path());
        test::write_bytes(path, std::vector<uint8_t>(content.begin(), content.end()));
        return path;
    }

    ResourceSettings resources() const
    {
        ResourceSettings result;
        result.root = dir.path() / "res";
        return result;
    }

    std::string load(const std::filesystem::path& path)
    {
        AssetManager manager(AssetSettings{}, resources());
        AssetHandle<CountedAsset> handle = test::load_now<CountedAsset>(manager, path);
        return manager.state(handle) == AssetState::Ready ? manager.get(handle).text : "<failed: " + manager.error(handle) + ">";
    }

    std::vector<std::filesystem::path> entries() const
    {
        std::vector<std::filesystem::path> found;
        if (std::filesystem::exists(dir.path() / "res" / "compiled"))
        {
            for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(dir.path() / "res" / "compiled"))
            {
                if (entry.is_regular_file())
                {
                    found.push_back(entry.path());
                }
            }
        }
        return found;
    }

    test::AssetTempDir dir;
};

std::vector<uint8_t> bytes_of(const std::string& text)
{
    return std::vector<uint8_t>(text.begin(), text.end());
}

} // namespace

TEST_CASE_FIXTURE(CacheFixture, "the importer runs once, then later loads hit the compiled entry")
{
    std::filesystem::path path = source("a.counted", "abc");
    CHECK(load(path) == "cba");
    CHECK(g_imports == 1);
    REQUIRE(entries().size() == 1);
    CHECK(entries().front().parent_path().filename() == "counted");
    CHECK(entries().front().extension() == ".oxcounted");

    CHECK(load(path) == "cba");
    CHECK(g_imports == 1);
    CHECK(g_instantiations == 2);
}

TEST_CASE_FIXTURE(CacheFixture, "changed source bytes, importer version and settings hash miss")
{
    std::filesystem::path path = source("a.counted", "abc");
    load(path);
    REQUIRE(g_imports == 1);

    source("a.counted", "abd");
    CHECK(load(path) == "dba");
    CHECK(g_imports == 2);

    g_version = 2;
    load(path);
    CHECK(g_imports == 3);

    g_settings = 99;
    load(path);
    CHECK(g_imports == 4);

    load(path);
    CHECK(g_imports == 4);
}

TEST_CASE_FIXTURE(CacheFixture, "the key excludes the source path")
{
    load(source("a.counted", "same"));
    REQUIRE(g_imports == 1);
    CHECK(load(source("moved/elsewhere.counted", "same")) == "emas");
    CHECK(g_imports == 1);
}

TEST_CASE_FIXTURE(CacheFixture, "corrupt and truncated entries fall back to importing")
{
    std::filesystem::path path = source("a.counted", "abc");
    load(path);
    std::filesystem::path entry = entries().front();
    std::vector<uint8_t> original = read_binary_file(entry);

    std::vector<uint8_t> flipped = original;
    flipped.back() ^= 0xFF;
    test::write_bytes(entry, flipped);
    CHECK(load(path) == "cba");
    CHECK(g_imports == 2);

    test::write_bytes(entry, std::vector<uint8_t>(original.begin(), original.end() - 1));
    CHECK(load(path) == "cba");
    CHECK(g_imports == 3);

    std::vector<uint8_t> bad_magic = original;
    bad_magic[0] ^= 0xFF;
    test::write_bytes(entry, bad_magic);
    CHECK(load(path) == "cba");
    CHECK(g_imports == 4);

    test::write_bytes(entry, {});
    CHECK(load(path) == "cba");
    CHECK(g_imports == 5);

    CHECK(load(path) == "cba");
    CHECK(g_imports == 5);
}

TEST_CASE_FIXTURE(CacheFixture, "a foreign format version, importer version or type tag is a miss")
{
    std::filesystem::path path = source("a.counted", "abc");
    load(path);
    std::filesystem::path entry = entries().front();
    const std::vector<uint8_t> original = read_binary_file(entry);

    const size_t offsets[] = { 4, 8, 12 };
    int32_t expected = 1;
    for (size_t offset : offsets)
    {
        std::vector<uint8_t> damaged = original;
        damaged[offset] ^= 0x01;
        test::write_bytes(entry, damaged);
        CHECK(load(path) == "cba");
        CHECK(g_imports == ++expected);
    }

    std::filesystem::path other = source("b.counted", "xyz");
    load(other);
    ++expected;
    REQUIRE(entries().size() == 2);
    std::vector<std::filesystem::path> both = entries();
    std::filesystem::path other_entry = both[0] == entry ? both[1] : both[0];
    test::write_bytes(other_entry, original);
    CHECK(load(other) == "zyx");
    CHECK(g_imports == ++expected);
}

TEST_CASE_FIXTURE(CacheFixture, "pruning removes entries no registered type can read and keeps current ones")
{
    std::filesystem::path path = source("a.counted", "abc");
    load(path);
    REQUIRE(g_imports == 1);
    std::filesystem::path folder = dir.path() / "res" / "compiled" / "counted";

    test::write_bytes(folder / "leftover.oxcounted.tmp123", { 1 });
    test::write_bytes(folder / "garbage.oxcounted", { 1, 2, 3 });
    test::write_bytes(folder / "legacy.bin", { 1 });
    std::filesystem::create_directories(dir.path() / "res" / "compiled" / "unknown");
    test::write_bytes(dir.path() / "res" / "compiled" / "unknown" / "x.oxunknown", { 1 });
    REQUIRE(entries().size() == 5);

    CHECK(load(path) == "cba");
    CHECK(g_imports == 1);
    REQUIRE(entries().size() == 1);

    g_version = 2;
    CompiledAssetStore store(dir.path() / "res" / "compiled", true);
    store.prune();
    CHECK(entries().empty());
}

TEST_CASE_FIXTURE(CacheFixture, "an unwritable compiled directory warns once and loading continues")
{
    std::filesystem::path blocker = dir.path() / "blocker";
    test::write_bytes(blocker, { 1 });
    ResourceSettings resources;
    resources.root = blocker / "res";
    std::filesystem::path first = source("a.counted", "abc");
    std::filesystem::path second = source("b.counted", "xyz");

    test::ClientLogCapture client;
    AssetManager manager(AssetSettings{}, resources);
    AssetHandle<CountedAsset> a = test::load_now<CountedAsset>(manager, first);
    AssetHandle<CountedAsset> b = test::load_now<CountedAsset>(manager, second);
    CHECK(manager.get(a).text == "cba");
    CHECK(manager.get(b).text == "zyx");

    std::vector<std::string> lines = client.lines();
    CHECK(std::count_if(lines.begin(), lines.end(), [](const std::string& line) { return line.find("not writable") != std::string::npos; }) == 1);
}

TEST_CASE_FIXTURE(CacheFixture, "either compiled switch disables reading and writing")
{
    AssetSettings assets_off;
    assets_off.compiled_enabled = false;
    ResourceSettings resources_off = resources();
    resources_off.compiled_enabled = false;
    std::filesystem::path path = source("a.counted", "abc");
    for (int32_t i = 0; i < 2; ++i)
    {
        AssetManager by_assets(assets_off, resources());
        test::load_now<CountedAsset>(by_assets, path);
        AssetManager by_resources(AssetSettings{}, resources_off);
        test::load_now<CountedAsset>(by_resources, path);
    }
    CHECK(g_imports == 4);
    CHECK(entries().empty());
}

TEST_CASE_FIXTURE(CacheFixture, "import_source is pure: the same input gives identical bytes")
{
    UniquePtr<IAssetImporter<CountedAsset>> importer = Registry<IAssetImporter<CountedAsset>>::create(".counted");
    REQUIRE(importer != nullptr);
    std::vector<uint8_t> input = bytes_of("deterministic");
    ImportInput request{ "x.counted", input.data(), input.size() };
    CHECK(importer->import_source(request) == importer->import_source(request));
}

TEST_CASE("CompiledAssetStore round-trips payloads and keys differ per field")
{
    test::AssetTempDir dir;
    CompiledAssetStore store(dir.path(), true);
    std::vector<uint8_t> source = bytes_of("source");
    CompiledAssetKey key = make_compiled_asset_key("kind", 1, 2, source.data(), source.size());

    std::vector<uint8_t> out;
    CHECK_FALSE(store.read(key, out));
    std::vector<uint8_t> payload = bytes_of("payload");
    store.write(key, payload.data(), payload.size());
    REQUIRE(store.read(key, out));
    CHECK(out == payload);

    store.write(key, nullptr, 0);
    REQUIRE(store.read(key, out));
    CHECK(out.empty());

    CHECK(store.entry_path(key) != store.entry_path(make_compiled_asset_key("kind", 2, 2, source.data(), source.size())));
    CHECK(store.entry_path(key) != store.entry_path(make_compiled_asset_key("kind", 1, 3, source.data(), source.size())));
    CHECK(store.entry_path(key) != store.entry_path(make_compiled_asset_key("kin", 1, 2, source.data(), source.size())));
    CHECK(store.entry_path(key) == store.entry_path(make_compiled_asset_key("kind", 1, 2, source.data(), source.size())));
}

TEST_CASE("compiled asset keys and entry names are stable")
{
    std::vector<uint8_t> source = bytes_of("source");
    CompiledAssetKey image = make_compiled_asset_key("image", 1, 2, source.data(), source.size());
    CHECK(image.source_hash == 0x76dbdc228f782db8ull);
    CHECK(hash_compiled_asset_key(image) == 0x99e45a7827652ddbull);
    CHECK(CompiledAssetStore("root", true).entry_path(image) == std::filesystem::path("root") / "image" / "99e45a7827652ddb.oximage");
    CHECK(hash_compiled_asset_key(make_compiled_asset_key("atlas", 3, 0, nullptr, 0)) == 0x65fcf829dee9b4cdull);
}

TEST_CASE("compiled asset types must be path-safe")
{
    CHECK_THROWS_AS((void)make_compiled_asset_key("", 1, 0, nullptr, 0), Error);
    CHECK_THROWS_AS((void)make_compiled_asset_key("Image", 1, 0, nullptr, 0), Error);
    CHECK_THROWS_AS((void)make_compiled_asset_key("a/b", 1, 0, nullptr, 0), Error);
    CHECK_THROWS_AS((void)make_compiled_asset_key("abcdefghijklmnopqrst", 1, 0, nullptr, 0), Error);
    CHECK_NOTHROW((void)make_compiled_asset_key("abcdefghijklmnopqrs", 1, 0, nullptr, 0));
}

TEST_CASE("registering a compiled type needs no store change")
{
    register_compiled_type("meshdummy", [] { return 7u; });
    CHECK(compiled_type_versions().at("meshdummy") == 7);

    test::AssetTempDir dir;
    CompiledAssetStore store(dir.path(), true);
    std::vector<uint8_t> payload = bytes_of("mesh");
    CompiledAssetKey key = make_compiled_asset_key("meshdummy", 7, 0, payload.data(), payload.size());
    store.write(key, payload.data(), payload.size());
    CHECK(store.entry_path(key).extension() == ".oxmeshdummy");
    store.prune();
    std::vector<uint8_t> out;
    CHECK(store.read(key, out));
}

TEST_CASE("resource and asset settings read their sections relative to the settings file")
{
    test::AssetTempDir dir;
    test::write_bytes(dir.path() / "oryx.yaml", bytes_of("resources:\n  root: res\n  compiled_enabled: false\nassets:\n  roots:\n    - content\n  compiled_enabled: false\n"));
    std::string flag = "--settings=" + (dir.path() / "oryx.yaml").string();
    std::string program = "app";
    std::vector<char*> args = { program.data(), flag.data() };
    load_settings({ 2, args.data() });

    const AssetSettings& settings = settings_of<AssetSettings>();
    REQUIRE(settings.roots.size() == 1);
    CHECK(settings.roots[0] == (dir.path() / "content").lexically_normal());
    CHECK_FALSE(settings.compiled_enabled);
    CHECK(settings.worker_threads == 0);
    const ResourceSettings& resources = settings_of<ResourceSettings>();
    CHECK(resources.root == (dir.path() / "res").lexically_normal());
    CHECK_FALSE(resources.compiled_enabled);
    CHECK(compiled_directory(resources) == (dir.path() / "res" / "compiled").lexically_normal());

    reset_settings();
    CHECK(settings_of<AssetSettings>().roots.empty());
    CHECK(settings_of<AssetSettings>().compiled_enabled);
    CHECK(settings_of<ResourceSettings>().root == std::filesystem::path("resources"));
    CHECK(settings_of<ResourceSettings>().compiled_enabled);
}

TEST_CASE("a settings file without a resources key resolves the default root beside the file")
{
    test::AssetTempDir dir;
    test::write_bytes(dir.path() / "oryx.yaml", bytes_of("resources:\n  compiled_enabled: true\n"));
    std::string flag = "--settings=" + (dir.path() / "oryx.yaml").string();
    std::string program = "app";
    std::vector<char*> args = { program.data(), flag.data() };
    load_settings({ 2, args.data() });
    CHECK(settings_of<ResourceSettings>().root == (dir.path() / "resources").lexically_normal());
    reset_settings();
}

TEST_CASE("Assets::manager refuses worker threads until a pool exists")
{
    test::AssetTempDir dir;
    test::write_bytes(dir.path() / "oryx.yaml", bytes_of("assets:\n  worker_threads: 2\n"));
    std::string flag = "--settings=" + (dir.path() / "oryx.yaml").string();
    std::string program = "app";
    std::vector<char*> args = { program.data(), flag.data() };
    load_settings({ 2, args.data() });

    Assets::reset();
    CHECK_THROWS_AS((void)Assets::manager(), Error);
    reset_settings();
    CHECK_NOTHROW((void)Assets::manager());
    Assets::use<AssetManager>(test::uncached_settings());
}
