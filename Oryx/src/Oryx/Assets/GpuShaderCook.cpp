#include "oxpch.h"
#include "Oryx/Assets/GpuShaderCook.h"

#include "Oryx/Assets/GpuShaderStore.h"
#include "Oryx/Core/Log.h"
#include "Oryx/Shaders/ShaderSettings.h"

namespace oryx
{

namespace
{

class ShaderCookCommandLine : public ICommandLineContributor
{
public:
    void declare(CommandLine& command_line) const override
    {
        command_line.flag("cook-shaders", "Compile every shader into the compiled-asset store for a cooked (Dist) run, then exit");
    }
};

OX_REGISTER_COMMAND_LINE(ShaderCookCommandLine, "shaders")

} // namespace

bool shader_cook_requested(const ParsedArgs& args)
{
    return args.has("cook-shaders");
}

ShaderCookResult run_shader_cook()
{
    ShaderSettings settings = settings_of<ShaderSettings>();
    if (effective_source_mode(settings) == ShaderSourceMode::Cooked)
    {
        settings.source_mode = ShaderSourceMode::FileThenEmbedded;
    }
    const ShaderSourceResolver sources(settings);
    ShaderCache cache;
    const ShaderCookResult result = cook_shaders(cache, sources.provider(), gpu_shader_store());
    OX_CORE_INFO("cooked {} shaders (map {:016x})", result.shaders, result.map_id);
    return result;
}

} // namespace oryx
