#include "doctest.h"

#include "ShaderTestSupport.h"

#include "Oryx/Shaders/ShaderSettings.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

constexpr const char* PS_SOURCE = R"msl(
#include "Oryx/Test/Part.msl"
fragment float4 provider_ps() { return float4(PART_VALUE); }
)msl";

std::filesystem::path make_temp_root()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "oryx_shader_provider_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "Test");
    return root;
}

void write_file(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream(path, std::ios::binary) << text;
}

class ProviderTestPS : public StaticShader<PixelShader>
{
public:
    using StaticShader::StaticShader;
};

const EmbeddedShaderRegistrar provider_source("/Oryx/Test/Provider.msl", PS_SOURCE, std::strlen(PS_SOURCE));
const EmbeddedShaderRegistrar part_source("/Oryx/Test/Part.msl", "#define PART_VALUE 0\n", 21);

OX_REGISTER_SHADER(ProviderTestPS, "/Oryx/Test/Provider.msl", "provider_ps", ShaderStage::Pixel)

} // namespace

TEST_CASE("Embedded provider returns registered sources and rejects unknown paths")
{
    register_embedded_shader("/Test/Embedded.msl", "// embedded\n");
    const EmbeddedShaderSourceProvider provider;
    const std::optional<ShaderSource> found = provider.load("/Test/Embedded.msl");
    REQUIRE(found.has_value());
    CHECK(found->text == "// embedded\n");
    CHECK(found->language == ShaderLanguage::MSL);
    CHECK_FALSE(provider.load("/Test/Missing.msl").has_value());
}

TEST_CASE("File provider maps the Oryx mount onto its root")
{
    const std::filesystem::path root = make_temp_root();
    write_file(root / "Test" / "File.msl", "// loose\n");
    const FileShaderSourceProvider provider(root);
    const std::optional<ShaderSource> found = provider.load("/Oryx/Test/File.msl");
    REQUIRE(found.has_value());
    CHECK(found->text == "// loose\n");
    CHECK_FALSE(provider.load("/Oryx/Test/Missing.msl").has_value());
    CHECK_FALSE(provider.load("/Other/Test/File.msl").has_value());
}

TEST_CASE("Layered provider prefers the primary and falls back to the secondary")
{
    const std::filesystem::path root = make_temp_root();
    write_file(root / "Test" / "Both.msl", "// disk\n");
    register_embedded_shader("/Oryx/Test/Both.msl", "// embedded\n");
    register_embedded_shader("/Oryx/Test/OnlyEmbedded.msl", "// only\n");
    const FileShaderSourceProvider file(root);
    const EmbeddedShaderSourceProvider embedded;
    const LayeredShaderSourceProvider layered(file, embedded);
    CHECK(layered.load("/Oryx/Test/Both.msl")->text == "// disk\n");
    CHECK(layered.load("/Oryx/Test/OnlyEmbedded.msl")->text == "// only\n");
    CHECK_FALSE(layered.load("/Oryx/Test/Nowhere.msl").has_value());
}

TEST_CASE("Source mode Auto resolves per build configuration and explicit modes win")
{
    ShaderSettings settings;
    settings.source_mode = ShaderSourceMode::Embedded;
    CHECK(effective_source_mode(settings) == ShaderSourceMode::Embedded);
    settings.source_mode = ShaderSourceMode::Auto;
#ifdef OX_DIST
    CHECK(effective_source_mode(settings) == ShaderSourceMode::Embedded);
#else
    CHECK(effective_source_mode(settings) == ShaderSourceMode::FileThenEmbedded);
#endif
}

TEST_CASE("Strict file mode fails on a missing file instead of falling back")
{
    ShaderSettings settings;
    settings.root = make_temp_root();
    settings.source_mode = ShaderSourceMode::File;
    register_embedded_shader("/Oryx/Test/Strict.msl", "// embedded\n");
    const ShaderSourceResolver strict(settings);
    CHECK_FALSE(strict.provider().load("/Oryx/Test/Strict.msl").has_value());
    settings.source_mode = ShaderSourceMode::FileThenEmbedded;
    const ShaderSourceResolver relaxed(settings);
    CHECK(relaxed.provider().load("/Oryx/Test/Strict.msl").has_value());
}

TEST_CASE("Embedded built-in shaders match the loose files")
{
    ShaderSettings settings;
    settings.source_mode = ShaderSourceMode::File;
    const ShaderSourceResolver files(settings);
    const EmbeddedShaderSourceProvider embedded;
    size_t checked = 0;
    for (const std::string& path : embedded_shader_paths())
    {
        if (path.rfind("/Oryx/Builtin/", 0) != 0 && path != "/Oryx/Common.msl")
        {
            continue;
        }
        const std::optional<ShaderSource> loose = files.provider().load(path);
        REQUIRE_MESSAGE(loose.has_value(), path);
        CHECK_MESSAGE(loose->text == embedded.load(path)->text, "stale embedded copy of " << path << "; run Oryx/tools/embed_shaders.py");
        ++checked;
    }
    CHECK(checked >= 5);
}

TEST_CASE("ShaderLibrary loads sources and includes through the provider and reloads transactionally")
{
    const std::filesystem::path root = make_temp_root();
    write_file(root / "Test" / "Provider.msl", PS_SOURCE);
    write_file(root / "Test" / "Part.msl", "#define PART_VALUE 1\n");
    const FileShaderSourceProvider files(root);

    NullRHI rhi;
    ShaderCache cache;
    ShaderLibrary library;
    const ShaderType& type = shader_type_of<ProviderTestPS>();
    library.compile(rhi, cache, type, files);
    const ShaderHash first = library.get<ProviderTestPS>()->hash();

    write_file(root / "Test" / "Part.msl", "#define PART_VALUE 2\n");
    library.reload(rhi, cache, type, files);
    CHECK(library.get<ProviderTestPS>()->hash() != first);

    const ShaderHash good = library.get<ProviderTestPS>()->hash();
    write_file(root / "Test" / "Part.msl", "#define PART_VALUE (\n struct { broken\n");
    CHECK_THROWS_AS(library.reload(rhi, cache, type, files), Error);
    CHECK(library.get<ProviderTestPS>()->hash() == good);
}

TEST_CASE("A missing source names the path")
{
    NullRHI rhi;
    ShaderCache cache;
    ShaderLibrary library;
    const FileShaderSourceProvider files(make_temp_root());
    std::string message;
    try
    {
        library.compile(rhi, cache, shader_type_of<ProviderTestPS>(), files);
    }
    catch (const Error& error)
    {
        message = error.what();
    }
    CHECK(message.find("/Oryx/Test/Provider.msl") != std::string::npos);
}
