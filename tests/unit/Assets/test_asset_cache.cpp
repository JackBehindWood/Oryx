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

    AssetSettings settings() const
    {
        AssetSettings result;
        result.cache_dir = dir.path() / "cache";
        return result;
    }

    std::string load(const std::filesystem::path& path)
    {
        AssetManager manager(settings());
        AssetHandle<CountedAsset> handle = test::load_now<CountedAsset>(manager, path);
        return manager.state(handle) == AssetState::Ready ? manager.get(handle).text : "<failed: " + manager.error(handle) + ">";
    }

    std::vector<std::filesystem::path> entries() const
    {
        std::vector<std::filesystem::path> found;
        if (std::filesystem::exists(dir.path() / "cache"))
        {
            for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(dir.path() / "cache"))
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

TEST_CASE_FIXTURE(CacheFixture, "the importer runs once, then later loads hit the cache")
{
    std::filesystem::path path = source("a.counted", "abc");
    CHECK(load(path) == "cba");
    CHECK(g_imports == 1);
    CHECK(entries().size() == 1);

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

TEST_CASE_FIXTURE(CacheFixture, "the cache key excludes the source path")
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

TEST_CASE_FIXTURE(CacheFixture, "an unwritable cache directory warns once and loading continues")
{
    std::filesystem::path blocker = dir.path() / "blocker";
    test::write_bytes(blocker, { 1 });
    AssetSettings settings;
    settings.cache_dir = blocker / "cache";
    std::filesystem::path first = source("a.counted", "abc");
    std::filesystem::path second = source("b.counted", "xyz");

    test::ClientLogCapture client;
    AssetManager manager(settings);
    AssetHandle<CountedAsset> a = test::load_now<CountedAsset>(manager, first);
    AssetHandle<CountedAsset> b = test::load_now<CountedAsset>(manager, second);
    CHECK(manager.get(a).text == "cba");
    CHECK(manager.get(b).text == "zyx");

    std::vector<std::string> lines = client.lines();
    CHECK(std::count_if(lines.begin(), lines.end(), [](const std::string& line) { return line.find("not writable") != std::string::npos; }) == 1);
}

TEST_CASE_FIXTURE(CacheFixture, "a disabled cache neither reads nor writes")
{
    AssetSettings disabled = settings();
    disabled.cache_enabled = false;
    std::filesystem::path path = source("a.counted", "abc");
    for (int32_t i = 0; i < 2; ++i)
    {
        AssetManager manager(disabled);
        test::load_now<CountedAsset>(manager, path);
    }
    CHECK(g_imports == 2);
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

TEST_CASE("AssetCache round-trips payloads and keys differ per field")
{
    test::AssetTempDir dir;
    AssetCache cache(dir.path(), true);
    std::vector<uint8_t> source = bytes_of("source");
    AssetCacheKey key = make_asset_cache_key("kind", 1, 2, source.data(), source.size());

    std::vector<uint8_t> out;
    CHECK_FALSE(cache.read(key, out));
    std::vector<uint8_t> payload = bytes_of("payload");
    cache.write(key, payload.data(), payload.size());
    REQUIRE(cache.read(key, out));
    CHECK(out == payload);

    cache.write(key, nullptr, 0);
    REQUIRE(cache.read(key, out));
    CHECK(out.empty());

    CHECK(cache.entry_path(key) != cache.entry_path(make_asset_cache_key("kind", 2, 2, source.data(), source.size())));
    CHECK(cache.entry_path(key) != cache.entry_path(make_asset_cache_key("kind", 1, 3, source.data(), source.size())));
    CHECK(cache.entry_path(key) != cache.entry_path(make_asset_cache_key("kin", 1, 2, source.data(), source.size())));
    CHECK(cache.entry_path(key) == cache.entry_path(make_asset_cache_key("kind", 1, 2, source.data(), source.size())));
}

TEST_CASE("AssetSettings reads its section relative to the settings file")
{
    test::AssetTempDir dir;
    test::write_bytes(dir.path() / "oryx.yaml", bytes_of("assets:\n  roots:\n    - content\n  cache_dir: derived\n  cache_enabled: false\n"));
    std::string flag = "--settings=" + (dir.path() / "oryx.yaml").string();
    std::string program = "app";
    std::vector<char*> args = { program.data(), flag.data() };
    load_settings({ 2, args.data() });

    const AssetSettings& settings = settings_of<AssetSettings>();
    REQUIRE(settings.roots.size() == 1);
    CHECK(settings.roots[0] == (dir.path() / "content").lexically_normal());
    CHECK(settings.cache_dir == (dir.path() / "derived").lexically_normal());
    CHECK_FALSE(settings.cache_enabled);
    CHECK(settings.worker_threads == 0);

    reset_settings();
    CHECK(settings_of<AssetSettings>().roots.empty());
    CHECK(settings_of<AssetSettings>().cache_dir == std::filesystem::path(".cache/assets"));
    CHECK(settings_of<AssetSettings>().cache_enabled);
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
