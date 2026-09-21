#include "oxpch.h"
#include "Oryx/Scripting/ScriptingLayer.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Events/ScriptEvent.h"
#include "Oryx/Scripting/ScriptRuntimeRegistry.h"

namespace oryx
{

namespace
{

std::string describe(const ScriptSource& source)
{
    return source.kind == ScriptSourceKind::Module ? "module '" + source.target + "'" : "script '" + source.target + "'";
}

void load_source(IScriptRuntime& runtime, const ScriptSource& source, bool reloading)
{
    try
    {
        reloading ? runtime.reload(source) : runtime.load(source);
        OX_CORE_INFO("ScriptingLayer: {} {} with the {} runtime.", reloading ? "reloaded" : "loaded", describe(source), runtime.language());
    }
    catch (const Error& error)
    {
        error.log();
    }
}

void report_unmatched(const ScriptSource& source, size_t runtime_count)
{
    if (source.kind == ScriptSourceKind::Module && runtime_count > 1)
    {
        OX_CORE_ERROR("ScriptingLayer: {} is ambiguous between {} script runtimes - skipping it.", describe(source), runtime_count);
        return;
    }

    if (source.kind == ScriptSourceKind::Module)
    {
        OX_CORE_WARN("ScriptingLayer: no runtime for {} - skipping it.", describe(source));
        return;
    }

    std::string extension = std::filesystem::path(source.target).extension().string();
    OX_CORE_WARN("ScriptingLayer: no runtime for {} - skipping {}.", extension.empty() ? "this file type" : extension, describe(source));
}

} // namespace

ScriptingLayer::ScriptingLayer(ScriptDiscoveryOptions options)
    : ScriptingLayer(std::move(options), ScriptRuntimeRegistry::create_all())
{
}

ScriptingLayer::ScriptingLayer(ScriptDiscoveryOptions options, std::vector<UniquePtr<IScriptRuntime>> runtimes)
    : Layer("ScriptingLayer")
    , m_options(std::move(options))
    , m_runtimes(std::move(runtimes))
{
}

void ScriptingLayer::attach()
{
    if (m_runtimes.empty())
    {
        OX_CORE_TRACE("ScriptingLayer: no script runtime is registered.");
    }

    sync_runtimes();
}

void ScriptingLayer::event(Event& event)
{
    EventDispatcher dispatcher(event);
    dispatcher.dispatch<ReloadScriptsEvent>([this](ReloadScriptsEvent&)
    {
        sync_runtimes();
        return false;
    });
}

std::vector<ScriptSource> ScriptingLayer::discover() const
{
    std::vector<ScriptFilePattern> patterns;
    for (const UniquePtr<IScriptRuntime>& runtime : m_runtimes)
    {
        for (const std::string& pattern : runtime->file_patterns())
        {
            patterns.push_back(ScriptFilePattern{ runtime->language(), pattern });
        }
    }

    std::vector<ScriptSource> sources = discover_scripts(m_options, patterns);

    if (m_runtimes.size() == 1)
    {
        for (ScriptSource& source : sources)
        {
            if (source.kind == ScriptSourceKind::Module)
            {
                source.language = m_runtimes.front()->language();
            }
        }
    }

    for (const ScriptSource& source : sources)
    {
        if (source.language.empty())
        {
            report_unmatched(source, m_runtimes.size());
        }
    }
    return sources;
}

void ScriptingLayer::sync_runtimes()
{
    std::vector<ScriptSource> sources = discover();

    for (UniquePtr<IScriptRuntime>& runtime : m_runtimes)
    {
        std::vector<const ScriptSource*> own_sources;
        for (const ScriptSource& source : sources)
        {
            if (source.language == runtime->language())
            {
                own_sources.push_back(&source);
            }
        }

        bool running = std::find(m_started.begin(), m_started.end(), runtime.get()) != m_started.end();
        if (!running && own_sources.empty())
        {
            continue;
        }

        if (running)
        {
            runtime->unload();
        }
        else
        {
            runtime->start();
            m_started.push_back(runtime.get());
        }

        for (const ScriptSource* source : own_sources)
        {
            load_source(*runtime, *source, running);
        }
    }
}

void ScriptingLayer::detach()
{
    while (!m_started.empty())
    {
        IScriptRuntime* runtime = m_started.back();
        m_started.pop_back();
        runtime->stop();
    }
}

} // namespace oryx
